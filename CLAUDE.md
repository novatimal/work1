# Part 1

## Обзор

Pivot Engine — кроссплатформенный (Windows/MSYS2 + Linux) 3D-движок на C++17 и OpenGL 4.6.
Один исполняемый файл совмещает редактор сцен (ImGui) и игровой рантайм; логика игры пишется
на Lua 5.4 через sol2. Вендоренные зависимости лежат прямо в репозитории (`imgui/`, `imguizmo/`,
`RmlUi/`, `JoltPhysics/`, `sol/`, `stb/`, `glad/`, `peplib/`, а также header-only `json.hpp`,
`miniaudio.h`, `tiny_gltf.h`, `tiny_obj_loader.h`, `mikktspace.*`); системные — через pkg-config
(glfw3, lua, freetype, fontconfig, liblz4; опционально FFmpeg).

## Сборка и запуск

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release      # Debug → -O0; иначе -O3 + LTO
cmake --build . -j$(nproc)
```

- Запускать **строго из корня репозитория** — все пути относительные (`cfg.json`,
  `resources.json`, `res/shaders/*`, `scripts/main.lua`):
  ```bash
  ./build/PivotEngine
  ```
- Опции CMake:
  | Опция | По умолчанию | Что делает |
  |---|---|---|
  | `-DUSE_RMLUI=OFF` | `ON` | отключает RmlUi и его сборку из исходников — заметно быстрее, когда работаешь не над UI |
  | `-DUSE_FFMPEG=OFF` | `ON` | отключает видеотекстуры из `.mp4` (анимированный `.gif` работает и без FFmpeg; если библиотеки не нашлись, сборка сама тихо обходится без них) |
  | `-DBUILD_TESTS=ON` | `OFF` | собирает `PhysicsTest` и `VfsTest` |
  | `-DDEBUG_INFO_FLAG=-g0` | `-g` | уровень отладочной информации; понижать при нехватке ОЗУ |
  | `-DPEP_REQUIRE_LZ4=OFF` | `ON` | разрешает собираться без liblz4 (тогда сжатые паки не читаются) |
- Полная пересборка долгая: `main.cpp` ~4.3k строк, `main.h` ~1.8k, `GraphicsEngineGL.cpp` ~6.6k,
  два файла биндингов по ~2.2k, плюс LTO в Release. Для итераций удобнее
  `-DCMAKE_BUILD_TYPE=Debug -DDEBUG_INFO_FLAG=-g0`.
- Полные списки пакетов для MSYS2 UCRT64 / Debian / Fedora — в `other/установки.txt`.
  Bullet, Vulkan и glslc больше не нужны.
- `launcher/` — отдельный проект (Qt6 + CURL + OpenSSL), исключён из основной сборки через
  `CMAKE_IGNORE_PATH`; собирается своим `cmake` в `launcher/`.

### Тесты

```bash
cmake -B build -DBUILD_TESTS=ON && cmake --build build
./build/PhysicsTest                       # дымовой тест обвязки Jolt
./tests/run_vfs_test.sh build/VfsTest     # пак собирается pepacker.py, читается C++
ctest --test-dir build                    # оба сразу
```

Lua-тесты (`tests/test_components.lua`, `test_shaders.lua`, `test_skeleton.lua`,
`test_shapekeys.lua`) запускаются самим движком — надо прописать нужный в `mainScript`
внутри `cfg.json`. Тесты, которым нужны прошедшие кадры, зовут `Engine.start()` и
доигрывают в `update()`.

В `tests/visual/` лежат визуальные тесты: сцена рендерится в PNG через
`Engine.screenshot()`, а `tests/visual/compare.py` разбирает снимки численно
(средняя яркость и контраст по зонам кадра). Подробнее — `tests/README.md`.

У `peplib` есть свои тесты (doctest), они собираются только когда peplib собирается как
самостоятельный проект (основной CMakeLists принудительно ставит `PEP_BUILD_TESTS OFF`):

```bash
cd peplib && cmake -B build_test -DPEP_BUILD_TESTS=ON && cmake --build build_test
./build_test/test_peplib
```

## Архитектура

Поток запуска: `main()` → монтирование `.pep`-паков (`vfs::setKey` + `vfs::mountDirectory`) →
`SceneEditorApp::run()` (main.cpp) → `loadGraphicsSettings()` (cfg.json) → `initWindow` →
`initEngines` → `initImGui` → `initScripting` (+ `bindScriptingAPI`) → `PhysicsEngine`/`AudioEngine`
→ `applyGraphicsSettings` → `setupResources()` (грузит `resources.json`) → `showSplashScreen()` →
загрузка `scripts/main.lua` (или `.luac`) → `mainLoop()`.

`SceneEditorApp` — центральный «божественный» класс: владеет всеми подсистемами, хранит сцену
(`std::vector<GameObject> gameObjects`), реализует ImGui-редактор, сериализацию PEMF и весь
биндинг Lua API. Это осознанное решение автора (tasklist.txt п.6: основной код держать
в main.cpp, максимум — вынести объявления в main.h). Не дроби его на мелкие файлы без явной
просьбы.

Раскладка по файлам (сложилась из-за нехватки памяти компилятора: sol2-шаблоны вместе
с остальным движком не влезали в один TU):

| Файл | Что внутри |
|---|---|
| `main.h` | includes, константы, макросы `BIND_PROP*`, свободные inline-функции, `class SceneEditorApp` целиком (включая inline-реализацию PEMF), `struct ObjectRef`, `struct ComponentRef` |
| `main.cpp` | реализация движка: цикл кадра, редактор, ресурсы, анимации, скелеты |
| `ScriptBindings.cpp` | `ComponentRef`, `bindComponentAPI`, `registerImGuiAPI`, `bindScriptingAPI`, `bindRmlUIAPI`, `bindPhysicsAPI` |
| `ScriptBindingsObject.cpp` | только `bindObjectAPI()` — usertype `ObjectRef` и таблица `Object` |

Подсистемы (`unique_ptr` внутри `SceneEditorApp`):

- **GraphicsEngineGL** — Clustered Forward (16×9×24 кластера, отбор света compute-шейдером),
  CSM на 4 каскада, теневой cubemap array для точечных и конусных источников, IBL/skybox
  (HDRI → irradiance/prefilter/BRDF LUT), SSAO, SSR, godrays,
  непрямое освещение через radiance cascades (пирамида каскадов с растущим
  интервалом и падающим пространственным разрешением; проход считается в
  конце кадра, а применяется на следующем — см. `res/shaders/rc_*.frag` и
  `res/shaders/lib/octahedral.glsl`),
  туман (пять режимов: exp², линейный, экспоненциальный, высотный — см.
  `res/shaders/lib/fog.glsl`; общий для геометрии, воды, партиклов и горизонта
  скайбокса), цветокоррекция, bloom, lens flare, motion blur, chromatic aberration,
  инстансинговые партиклы и отдельный distortion-проход, render-to-texture камеры (слоты 1..31),
  видеотекстуры, GPU-скиннинг, shape keys (morph targets из `.glb`, дельты в SSBO),
  отладочная отрисовка костей, снимок кадра в PNG (`saveScreenshot`). Шейдерные программы регистрируются через
  `addShaderProgram(num, vert, frag, [geom])` и компилируются в `startGraphics()`.
- **PhysicsEngine** — **Jolt Physics** (`JoltPhysics/`, PIMPL). Bullet удалён полностью.
  Симуляция идёт фиксированным шагом 1/60 с (аккумулятор внутри `stepSimulation`,
  не больше 8 шагов догона за кадр) — иначе поведение пружин и стопок зависело бы
  от FPS. Debug-отрисовка через `DebugRendererSimple`, режимы см. `setDebugMode`.
  Примитивы капсулы, цилиндра и конуса ориентированы по Z.
  Поверх базовых тел есть контроллер персонажа (`CharacterVirtual`), регдоллы,
  мягкие тела и ткань, транспорт, CCD и расширенный набор констрейнтов —
  подробности и примеры в `other/physics.md`.
- **ScriptEngine** — Lua 5.4 + sol2 (`SOL_ALL_SAFETIES_ON`).
- **AudioEngine** — miniaudio поверх VFS (собственный `ma_vfs`); 2D/3D-звук, окружения
  (`AudioEnvironment`), доплер, LPF по дистанции.
- **RmlUiSystem** — RmlUi; документы в `scripts/*.rml` + `*.rcss`. Собирается только при
  `USE_RMLUI=ON`.
- **NetworkEngine** — небольшой сетевой слой, доступен из Lua как таблица `Network`.
- **VirtualFS** (`VirtualFS.h/.cpp`) — VFS поверх peplib: `vfs::read/readText/size/exists`.
  Все загрузчики движка ходят только через неё; если файла нет в паках, он читается из обычной
  ФС (удобно при разработке). Запись всегда идёт мимо паков.
- **VideoDecoder** — FFmpeg (опционально) + анимированный GIF через stb.
  Наружу видеотекстуры выглядят обычными `TextureHandle`: старший бит хендла
  (`VIDEO_HANDLE_FLAG`) говорит «это видео», биты 0..7 — номер слота (их 16),
  биты 8..30 — поколение слота (защита от хендла на уже выгруженное видео).
  Разрешает хендл в GL-текстуру единая точка `resolveTextureId`. Тип файла
  определяется по расширению, а статичный `.gif` уходит в обычные текстуры.
  Из Lua — таблица `Video`: `getHandle/isVideo/play/pause/setLoop/setSpeed/
  seek/getTime/getDuration/isPlaying`; все они принимают и имя ресурса,
  и хендл (в горячем коде получи хендл один раз через `Video.getHandle`).
  Кадры отдаются перевёрнутыми по вертикали — как и все текстуры движка
  (`stbi_set_flip_vertically_on_load(true)`, строка 0 внизу). PTS
  нормализуются по `start_time` потока: у MPEG-TS они абсолютные, и без
  этого ролик застывал на первом кадре. Формат определяется по данным,
  а не по расширению: своё IO делает имя файла подсказкой, и `.mp4`
  с содержимым MPEG-TS не открывался. Лог FFmpeg идёт через свой
  обработчик, который схлопывает повторы.
- **peplib** — библиотека паков `.pep` (шифрование xoshiro256** + опциональный LZ4 **block**).

### Объектная модель

Сцена — плоский `std::vector<GameObject>` (GameObject.h). Объект держит **список** компонентов:

```cpp
struct Component {
    ComponentVariant data;
    glm::vec3 localPosition;      // сдвиг компонента внутри объекта
    glm::vec3 localRotation;
    bool enabled = true;
    glm::vec3 worldPosition;      // считается движком каждый кадр
    glm::quat worldRotationQuat;
};
```

Типы компонентов: `MeshComponent`, `PhysicsBodyComponent`, `LightComponent`,
`ConstraintComponent`, `ScriptComponent`, `CameraComponent`, `SoundComponent`,
`ParticleEmitterComponent`, `AnimationComponent`, `SkeletonComponent`,
`SingleParticleComponent`, `CustomComponent`. Строковые имена (`"mesh"`, `"physics"`, …)
возвращает `componentTypeName()` — они же используются в Lua API и при разборе PEMF.

Доступ: `getComponent<T>(go)` / `go.get<T>()` (первый компонент типа),
`go.find<T>()` (обёртка `Component*` — нужна, когда важны `enabled` и локальный сдвиг),
`go.addComponent(...)`, `go.removeComponents<T>()`; всех компонентов типа сразу
в C++ нет — перебирай `go.components` через `std::get_if<T>`. В Lua для этого
есть `obj:getComponents(тип)`. Подробности и правила перевода скриптов —
`other/components.md`.

У каждого компонента есть `uid` — устойчивый идентификатор в пределах сессии
(в `.pemf` не пишется). На него опирается `ComponentRef` из Lua: ссылаться на
компонент по индексу нельзя, удаление соседа сдвигает вектор.

Компонент по строковому имени типа создаёт свободная функция `makeComponentByName()`
(GameObject.h) — одна таблица на весь движок: её используют `addGameObject`,
`obj:addComponent` из Lua и кнопка «Добавить компонент» в редакторе.
Удаление идёт через `SceneEditorApp::removeComponentAt`, а оно зовёт
`releaseComponentResources` — иначе тело осталось бы в Jolt невидимым препятствием,
а звук играл бы в пустоте. Инспектор перебирает **все** компоненты объекта
(`drawComponentsUI`), а редакторы отдельных типов (`draw*ComponentUI`) правят
переданный компонент, а не первый подходящий.

**Деформация меша мягким телом.** `Physics.SoftBody.createFromObject(obj, opts)`
делает из меша объекта мягкое тело и привязывает деформацию обратно к этому
же мешу: дальше фаза 6.5 кадра каждый кадр переносит позиции частиц в вершины
и пересчитывает нормали. Меш объекта при этом заменяется на его **персональную
копию** (`GraphicsEngineGL::createDeformableMesh`) — деформация общего ресурса
поехала бы по всем объектам с тем же мешем. Вершины свариваются по позиции:
меш рендера дублирует их на швах UV, и без сварки ткань расползается по швам.
Отвязка — `Physics.SoftBody.unbindMesh(obj)`, она возвращает исходный меш.

**Регдолл со скиннингом.** Кость может управляться физическим телом:
`Bone::drivingBody` + поправка `bodyToBone`, посчитанная в момент привязки
(поэтому персонаж не «дёргается» при переходе в регдолл). Такая кость берёт
позу прямо из мировой трансформации тела, минуя иерархию костей и анимацию.
Привязка: `Physics.Ragdoll.bindToSkeleton(rag, obj[, {часть = кость}])` —
без таблицы части ищутся по совпадению имени с костью; поштучно —
`obj:driveBoneFromBody(имя_кости, хендл_тела)` и `obj:releaseBone(имя)`.

Иерархия: `position`/`rotation`/`rotationQuat` — мировые, `localPosition`/`localRotation` —
локальные относительно `parentId`, `attachToParent` включает пересчёт. `childrenIds` —
денормализованный кэш (обновляется автоматически при `autoUpdateChildren`, иначе вручную из
скрипта), `isHierarchyStatic` — подсказка для оптимизаций. Поиск объекта по id идёт через
кэш `m_objectIndexById`; любое структурное изменение `gameObjects` обязано звать
`invalidateObjectIndex()`.

Ресурсы адресуются непрозрачными `Handle = uint32_t` (EngineTypes.h), `0 == INVALID_HANDLE`.
Владельцы: `GraphicsEngineGL` (mesh/texture/material/video), `PhysicsEngine` (body/constraint).
`SceneEditorApp` держит параллельные списки `availableMeshes`/`availableMaterials`/... для
разрешения имя ↔ хендл.

### Порядок кадра (`SceneEditorApp::mainLoop`)

Девять фаз, порядок принципиален:

1. синхронизация иерархии и локальных трансформов (+ `updateComponentTransforms`)
2. push кинематики (`mass == 0`) в физику
3. шаг физики + pull динамики + события коллизий
4. update скриптов и аудио
5. анимации (включая AnimGraph)
6. скелеты (глобальные позы костей → BoneSSBO), `updateSkeletonPose`
7. CPU-симуляция партиклов
8. визуализация кабелей/пружин
9. UI и рендер (`drawFrame`)

Следствие: изменения, сделанные из Lua (фаза 4), доходят до физики только на следующем кадре.
Поза костей — исключение: `updateSkeletonPose` зовётся ещё и при создании рига, поэтому
`getBoneWorldPosition` работает уже в первом `update()`, а не со второго кадра.

### Конвенции

- Правая система координат, **Z вверх** (`Vector3.up == (0,0,1)`, `forward == (0,1,0)`), единицы СИ
  (метры, килограммы, секунды, канделы). Углы в структурах и API — градусы, внутри glm — радианы
  (`GLM_FORCE_RADIANS`). Цвет — RGBA4F.
- Комментарии и лог-сообщения в коде — на русском; придерживайся этого. Отступы в `main.cpp` и
  `GraphicsEngineGL.cpp` смешанные (табы и пробелы) — следуй локальному стилю правимого места.
- GPU-структуры (`SceneUniformBufferObject` в Common.h, `SceneLightSSBO` /
  `ClusterLightInfoSSBO` в GraphicsEngineGL.h) обязаны совпадать по раскладке std140/std430
  с GLSL в `res/shaders/`. Есть `static_assert`-ы на размер/выравнивание — правь структуру
  и шейдер вместе.
- Общие куски GLSL вынесены в `res/shaders/lib/*.glsl` и подключаются через собственный
  `#include`-препроцессор в `compileOrLoadShader`.
- SSR (`res/shaders/ssr.frag`) марширует луч с РАСТУЩИМ шагом: `ssrThickness` — это
  допуск попадания, а не длина шага, и привязка шага к нему однажды уже обрезала
  дальность луча до 6.4 м. Результат подмешивается «over» с предумноженной альфой
  (`GL_ONE, GL_ONE_MINUS_SRC_ALPHA`), а не аддитивно: иначе отражение только
  осветляет поверхность и никогда её не заменяет.
- Локации униформ кэшируются в `m_uniformLocs` (GraphicsEngineGL.h): новый uniform → поле
  в структуре + строка в `cacheUniformLocations()`.
- `compileOrLoadShader` умеет грузить и текстовый GLSL, и `.spv` (`glShaderBinary` +
  `glSpecializeShader`), различая по расширению. Пути к шейдерам задаются структурой `ShaderPaths`.
- Константы лимитов (`MAX_LIGHTS_SSBO`, `MAX_LIGHTS_PER_CLUSTER`, `CLUSTER_*`, `NUM_CASCADES`) —
  в Common.h и продублированы в шейдерах.
- Все загрузки файлов идут через `vfs::*`, а не через `std::ifstream`/`fopen`/`stbi_load` —
  иначе ресурс перестанет находиться в паке.

## Форматы данных

- **cfg.json** — `QualitySettings` (Common.h), параметры splash-экрана, секция
  `"shaderprograms"` (номер → vert/frag/geom), настройки тумана, godrays, цветокоррекции
  и radiance cascades; читается при старте. Размер окна задаётся
  `graphics.windowWidth`/`windowHeight` (0 — константы `WIDTH`/`HEIGHT` из Common.h).
- **resources.json / resourcesRelease.json** — манифест ресурсов (`textures`, `meshes`,
  `materials`, `gltfmats`, `sounds`, ...). Грузится `loadResourcesFromJson()`,
  из Lua — `Resources.load(path)`.
- **.pemf** (Pivot Engine Map Format) — бинарная сцена, magic `"PEMF"`, **версия 16**;
  `saveScene()` / `loadScene()` в main.h, типы компонентов — enum `PemfComponentType`,
  хелперы `pemfWrite*` / `pemfRead*`. Карты лежат в `res/maps/`.
  Обратной совместимости нет: при добавлении полей поднимай версию, обновляй обе стороны
  сериализации И python-инструменты (`other/pemf_format.py` — общий разбор,
  `other/pemf_migrate.py` — миграция, `other/pemfview.py`, `other/checkres.py`).
  В v16 у меша добавились веса форм (shape keys).
- **.pecf** — коллайдеры, **версия 3**, пишутся `PhysicsEngine::saveCollider`; экспорт из Blender —
  `other/colliderexport.py`, миграция старых — `other/pecf_migrate.py`
  (модель экспортируется как +Z up / +Y forward). В версии 3 капсула, цилиндр
  и конус ориентированы по Z, как и всё остальное в Z-up движке, поэтому
  экспортёр больше не запекает в поворот компенсацию `rx -= 90`.
- **.peaf** — анимации, **версия 5** (в v5 после обычных треков идут костевые);
  экспорт `other/animexport.py`, правка `other/peafadder.py`, просмотр `other/peafview.py`.
- **.pep** — пак ассетов peplib; инструмент `peplib/tools/pepacker.py`
  (`pack`/`unpack`/`list`, ключ через `-k 0x...`). Виртуальные пути считаются относительно
  `--root`, поэтому паки для движка собирают из корня репозитория:
  `pepacker.py pack -k 0x... -o game.pep --root . res scripts`.
  Ключ, которым движок открывает паки, задаётся константой `PACK_ENCRYPTION_KEY` вверху
  `main.h`; каталоги для автомонтирования — `PACK_DIRECTORIES` там же.
- **Скрипты** — `.lua` или скомпилированный байткод `.luac`
  (`Engine.compileScriptFromFileToFile`).

## Lua API

Точка входа — `scripts/main.lua` / `scripts/main.luac`.

Глобальные колбэки главного скрипта (вызываются через `ScriptEngine::safeCall`):
`update(dt)`, `onStart()`, `onStop()`, `onKeyPressed(key)`, `onKeyReleased(key)`.
Скрипты начинают исполняться только после `Engine.start()`.

Регистрация API разделена:
- `ScriptEngine::registerAPI` (ScriptEngine.cpp) — типы `Vector2/3/4`, `Color`, `NumSequence`,
  `Gradient`, таблицы `Input`, `Physics`, `Network`, `task`, функции `spawn`/`wait`/`delay`,
  переопределённые `print`/`warn`/`error`.
- `SceneEditorApp::bindScriptingAPI` (ScriptBindings.cpp) — `Engine`, `Sound`, `Sun`, `Material`,
  `Resources`, `RmlUi`, `ColGroup`, `Constraint`, `Json`, `ImGui`, `Gizmo`, `Graphics`, `Physics`,
  `Video`.
- `SceneEditorApp::bindObjectAPI` (ScriptBindingsObject.cpp) — `Object` и тип `ObjectRef`.

Подтаблицы физики живут под `Physics.*`: `Physics.ColGroup`, `Physics.Constraint`,
`Physics.Character`, `Physics.Ragdoll`, `Physics.SoftBody`, `Physics.Vehicle`.
Старые глобальные имена (`Ragdoll`, `SoftBody`, …) оставлены псевдонимами
**на те же таблицы** — существующие скрипты работают, но в новом коде
пользуйся строгой формой.
- `SceneEditorApp::bindComponentAPI` (ScriptBindings.cpp) — тип `ComponentRef`.

`ObjectRef` — прокси к `GameObject` по id. Свойства навешиваются макросами
`BIND_PROP` / `BIND_PROP_VEC2/3/4` (объявлены в начале main.h): добавление нового поля компонента
в Lua — это, как правило, одна строка в соответствующем блоке. Плоские свойства (`obj.mesh`,
`obj.mass`, …) всегда обращаются к **первому** компоненту подходящего типа; до остальных
добираются через `obj:getComponent(тип, номер)` / `obj:getComponents(тип)`.

`ScriptComponent` исполняется как отдельная корутина в изолированном `sol::environment`
с `lua_sethook` на лимит инструкций (`m_maxInstructionsPerFrame = 8000`) — защита от `while true do`.
Внутри такого скрипта `wait()` делает `lua_yield`. См. `ScriptEngine::registerScriptObject` /
`updateScriptObjects`.

**Все** Lua-функции, принимающие путь (`Engine.readFile/writeFile/appendFile`,
`Engine.loadScene/saveScene`, `Engine.mountPack/mountPackDirectory/fileExists`,
`Engine.compileScriptFromFileToFile`, `Resources.load`, `ImGui.loadFont`) проверяют его через
`SceneEditorApp::isPathSafe` (запрет `..`, `:` и абсолютных путей) — не ослабляй её.

## Инструменты в other/ (Python)

- `pemf_format.py` — общий разбор и запись `.pemf` (версии 12–16), база для остальных
- `pemf_migrate.py` — миграция карт на актуальную версию (с бэкапами `*.bak`)
- `pemfview.py` — просмотр и дамп `.pemf`-сцен
- `checkres.py` — проверка, что все ресурсы карты присутствуют в `resources.json`
- `pecf_migrate.py` — миграция коллайдеров `.pecf` на актуальную версию 3
- `colliderexport.py`, `animexport.py` — экспортёры из Blender (`.pecf`, анимации)
- `genResJson.py`, `updateSounds.py`, `allflac.py` — генерация/обновление секций `resources.json`
- `obfuscator.py`, `peafadder.py`, `peafview.py`
- `components.md`, `animgraph.md`, `physics.md` — документация по компонентной системе,
  графу анимаций и физике (персонаж, регдоллы, ткань, транспорт, CCD, констрейнты)

## Планы и незавершённые части

`tasklist.txt` — роадмап от автора; все 12 пунктов реализованы. Осознанно не доделано:

- подсистема occlusion-запросов (`beginOcclusionQuery`/`isVisible`/`updateOcclusionResults`)
  написана, но в `drawFrame` не подключена: отсечение делают пирамида видимости и кластеризация.
  Чтобы включить, нужен depth-prepass и кадр задержки на чтение результатов.
- `pep::PepStream` собирается, но движком не используется — API «про запас».

# Part 2 (components)
# Компонентная система Pivot Engine

Документ описывает, как устроены компоненты после перехода на множественные
компоненты у объекта (формат карт `.pemf` версии 16), и как переводить на них
скрипты.

## Модель

```
GameObject
├── id, name, enabled
├── position / rotation / rotationQuat      — мировая трансформация
├── localPosition / localRotation, parentId — иерархия (child-parent)
├── scale
└── components: vector<Component>
        ├── data          — один из ComponentVariant
        ├── localPosition / localRotation   — сдвиг компонента внутри объекта
        ├── worldPosition / worldRotationQuat — считается движком каждый кадр
        └── enabled       — выключенный компонент не рендерится и не обновляется
```

Объект больше не «является» компонентом: он их **держит**. На одном объекте
могут висеть меш, физическое тело, свет и звук одновременно, у каждого свой
сдвиг относительно объекта. Иерархия `parentId` + локальные координаты
сохранена: она удобна там, где части двигаются независимо (руки персонажа,
башня танка), а компоненты — там, где части двигаются вместе.

### Типы компонентов

| Имя в API         | Структура C++                | Назначение                          |
|-------------------|------------------------------|-------------------------------------|
| `mesh`            | `MeshComponent`              | всё для отрисовки                   |
| `physics`         | `PhysicsBodyComponent`       | масса, форма, коллизии              |
| `light`           | `LightComponent`             | источник света                      |
| `constraint`      | `ConstraintComponent`        | связь между телами                  |
| `script`          | `ScriptComponent`            | Lua-скрипт объекта                  |
| `camera`          | `CameraComponent`            | render-to-texture камера            |
| `sound`           | `SoundComponent`             | 2D/3D-звук                          |
| `particle`        | `SingleParticleComponent`    | одиночная частица                   |
| `particleemitter` | `ParticleEmitterComponent`   | эмиттер частиц                      |
| `animation`       | `AnimationComponent`         | клипы и граф состояний              |
| `skeleton`        | `SkeletonComponent`          | кости для скиннинга                 |
| `custom`          | `CustomComponent`            | произвольные параметры и события    |

## Что изменилось для скриптов

Старое `BodyComponent` разделено на `MeshComponent` и `PhysicsBodyComponent`.
Поля разошлись так:

* **меш** — `mesh`, `material`, `paintColor`, `modelOffset`, флаги теней и
  прозрачности, `isSkinnedMesh`, `skeletonId`, локальный AABB;
* **физика** — `mass`, `physicsEnabled`, `collisionEnabled`, `shapeType`,
  `collisionDimensions`, `collisionGroup`/`collisionMask`, `colliderFile`,
  составные части коллайдера.

### Совместимость

`Object.new("body", name)` по-прежнему работает и создаёт **оба** компонента —
меш и физическое тело. Плоские свойства объекта (`obj.mesh`, `obj.mass`,
`obj.castShadow`, ...) обращаются к первому компоненту подходящего типа, поэтому
старые скрипты продолжают работать без правок.

### Как переводить скрипты

**1. Просите ровно то, что нужно.** Декорациям (гильзы, следы от пуль,
вьюмодель, второй слой скина) физическое тело не нужно:

```lua
-- было
local hole = Object.new("body", name)
hole.mesh = "Plane"
hole.physicsEnabled = false

-- стало: физический компонент вообще не создаётся
local hole = Object.new("mesh", name)
hole.mesh = "Plane"
```

Обратное тоже верно: чистому триггеру или хитбоксу не нужен меш —
`Object.new("physics", name)`.

**2. Кэшируйте ссылку на компонент в горячем коде.** Плоское свойство каждый раз
ищет компонент по типу; `ComponentRef` — нет:

```lua
local emitter = obj:getComponent("particleemitter")
function update(dt)
    emitter.rate = 40 + 20 * math.sin(Scene.getTime())
end
```

**3. Складывайте связанные части в один объект.** Лампа = меш плафона + свет со
сдвигом вверх, а не два объекта с иерархией:

```lua
local lamp = Object.new("mesh", "Lamp")
lamp.mesh = "LampShade"
local light = lamp:addComponent("light")
light.localPosition = vec3(0, 0, -0.3)
light.intensity = 400
```

**Но только если частям не нужен свой масштаб.** У компонента есть локальный
сдвиг и поворот, а собственного масштаба нет. У прикреплённого объекта
масштаб перемножается с родительским (`newScale = scale * localScale`),
а вот смещение НЕ масштабируется ни там, ни там — формула одна и та же
(`position + rotation * localPosition`). Поэтому свет, звук и частицу
на вращающейся платформе можно смело делать её компонентами, а коробку
со своим размером — нельзя, она потеряет масштаб. Пример обоих случаев
рядом — в `scripts/test/test3.lua`.

## API компонентов в Lua

Методы `ObjectRef`:

| Метод                                   | Что делает                                        |
|-----------------------------------------|---------------------------------------------------|
| `obj:addComponent(тип)`                 | создаёт компонент, возвращает `ComponentRef`      |
| `obj:getComponent(тип [, номер])`       | компонент по типу (номер среди однотипных, с 1)   |
| `obj:getComponents([тип])`              | таблица всех компонентов (либо только этого типа) |
| `obj:removeComponent(ref или тип)`      | удаляет компонент, возвращает `true`/`false`      |
| `obj:componentCount()`                  | сколько компонентов на объекте                    |
| `obj:componentTypes()`                  | таблица имён типов по порядку                     |

`addComponent` и `removeComponent` работают через те же `addComponentByName` /
`removeComponentAt`, что и кнопки редактора, поэтому ведут себя одинаково:

* добавленный меш сразу получает куб и материал по умолчанию — иначе он
  невидим, и выглядит это как «ничего не произошло»;
* удаление компонента **освобождает его ресурсы**: тело убирается из Jolt,
  констрейнт разрывается, скрипт снимается с исполнения, звук останавливается.
  Раньше `removeComponent` просто стирал элемент из вектора, и тело оставалось
  в симуляции невидимым препятствием.

Свойства `ComponentRef`: `type`, `enabled`, `localPosition`, `localRotation`,
`worldPosition` (только чтение) и свойства самого компонента — те же имена, что
у плоских свойств объекта (`mesh`, `material`, `mass`, `shapeType`, `intensity`,
`rate`, `isSkinnedMesh`, `skeletonId`, ...). Правка `localPosition` /
`localRotation` сразу пересчитывает мировые координаты, ждать кадра не нужно.

У звукового компонента есть `play()` и `stop()`: без них добавленный через
`addComponent("sound")` звук невозможно было запустить — имя и громкость
выставить давали, а включить нет. 3D-звук идёт из **мировой** позиции
компонента, то есть с учётом локального сдвига: динамик на объекте слышно
оттуда, где он стоит.

`color` разбирается по типу компонента: у света это `vec3`, у частицы и
связи — `vec4`.

У самого `ObjectRef` рядом с `position`/`rotation` (они переключаются на
локальные при `attachToParent`) есть явные:

| Свойство | Что означает |
|---|---|
| `localPosition`, `localRotation` | всегда смещение относительно родителя |
| `worldPosition`, `worldRotation` | всегда мировые координаты, только чтение |

## Редактор компонентов

Инспектор (окно **Scene Objects**) показывает **все** компоненты объекта
списком, а не первый каждого типа: у каждого свой заголовок с номером,
галка включения, локальный сдвиг с поворотом, справка о мировых координатах
и кнопка `x` для удаления. Внизу — выпадающий список типов и кнопка
«Добавить компонент». Однотипные компоненты (два источника света, например)
правятся независимо.

Окна редактора и выделение доступны из скрипта — этим собирают нужную
раскладку, в том числе для визуальных тестов:

| Функция | Что делает |
|---|---|
| `Engine.debugUI(вкл)` / `Engine.isDebugUI()` | весь отладочный UI |
| `Engine.showWindow(имя, вкл)` / `Engine.isWindowShown(имя)` | окно по имени: `scene`, `graphics`, `stats`, `resources`, `console` |
| `Engine.selectObject(obj или id)` | выделить объект; `0` — снять выделение |
| `Engine.getSelectedObject()` | выделенный объект или `nil` |

Это именно функции, а не свойства: `Engine` — обычная таблица Lua, и
`sol::property` на ней не работает (чтение вернуло бы функцию, а присваивание
просто затёрло бы поле, ничего не переключив).

## Shape keys (morph targets)

Формы — это наборы смещений вершин относительно базовой геометрии; в `.glb`
они лежат в `primitive.targets`, имена берутся из `mesh.extras.targetNames`
(так их пишет Blender). Загружаются автоматически вместе с мешем.

Дельты хранятся **у меша** (один SSBO на меш), а веса — **у объекта**
(`MeshComponent::shapeKeyWeights`). Поэтому одну и ту же модель можно поставить
в сцену несколько раз с разными выражениями лица.

| Метод | Что делает |
|---|---|
| `obj:shapeKeys()` | таблица имён форм по порядку |
| `obj:shapeKeyCount()` | сколько форм у меша |
| `obj:getShapeKey(имя)` | вес формы; `0` для неизвестного имени |
| `obj:setShapeKey(имя, вес)` | задаёт вес; `false`, если такой формы нет |
| `obj:clearShapeKeys()` | все формы в ноль |

```lua
local head = Object.new("mesh", "Head")
head.mesh = "CharacterHead"
for i, name in ipairs(head:shapeKeys()) do print(i, name) end
head:setShapeKey("Smile", 0.7)
head:setShapeKey("BrowUp", 1.0)
```

Вес не ограничен диапазоном `[0..1]` — в glTF его тоже не ограничивают,
а перегиб формы (вес больше единицы или отрицательный) это обычный приём для
утрирования мимики. Формы применяются **до** скиннинга: они правят базовую
геометрию, а кости уже двигают результат. Тени их учитывают.

Ограничения:

- смешивается не больше `GraphicsEngineGL::MAX_SHAPE_KEYS` (64) форм на меш;
- объект с ненулевой формой выпадает из инстансинга — веса живут на объекте,
  а инстансы делят один вызов отрисовки. Пока все веса нулевые, инстансинг
  работает как обычно;
- смена `obj.mesh` сбрасывает веса: у нового меша формы другие;
- веса сохраняются в карту начиная с `.pemf` версии 16.

В инспекторе меша формы показываются отдельным блоком **Shape Keys** —
ползунок на каждую и кнопка сброса.

## Скелеты и кости

`Object.newSkeleton(имяМеша, имяМатериала)` собирает риг из `.glb`: создаёт
объект с `SkeletonComponent` и вешает на него по объекту-сабмешу на каждую
часть модели (`имяМеша` и всё, что начинается с `имяМеша_`). Сабмеши помечаются
`isSkinnedMesh`, ссылаются на скелет через `skeletonId` и считаются
непрозрачными: у скин-текстур почти всегда есть альфа-канал, а прозрачные тела
не попадают в проход теней. Для настоящего «призрака» снимите **Force Opaque**
в инспекторе меша.

| Метод скелета | Что делает |
|---|---|
| `skel:boneCount()` | сколько костей в риге |
| `skel:getBone(i)` | таблица `{name, parent, localPosition, localRotation, localScale}` либо `nil` |
| `skel:findBone(имя)` | номер кости или `-1` |
| `skel:setBonePose(i, pos, rot [, scale])` | локальная поза кости; `true`, если номер верный |
| `skel:getBoneWorldPosition(i)` | позиция кости в мире |
| `skel:getBoneWorldRotation(i)` | поворот кости в мире, градусы |

Кости нумеруются с нуля, `parent == -1` — корень. Внутри движка «нет родителя»
это `Bone::NO_BONE_PARENT` (`0xFFFFFFFF`), а не `INVALID_HANDLE`: последний
равен нулю, а кость 0 — обычная кость, и её дети иначе считались бы корнями.

Мировые позиции берутся из `boneGlobalPose`. Она пересчитывается в фазе 6 кадра
и один раз сразу при создании рига, так что читать её можно уже в первом
`update()`. По ней крепят предметы к рукам и головам:

```lua
local skel = Object.newSkeleton("SteveRig", "navalniySkin1")
local hand = skel:findBone("Arm_R")
function update(dt)
    if hand ~= -1 then
        sword.position = skel:getBoneWorldPosition(hand)
        sword.rotation = skel:getBoneWorldRotation(hand)
    end
end
```

Костевые треки живут в `AnimationComponent` рядом с обычными:

| Метод анимации | Что делает |
|---|---|
| `anim:createClip(имя, длительность, loop)` | новый клип |
| `anim:addBoneTrack(клип, trackId, boneId)` | трек для кости; `false`, если клипа нет |
| `anim:addBoneKeyframe(клип, trackId, boneId, время, pos, rot, scl)` | кадр; `false`, если трека нет |
| `anim:playClip(имя)` | запуск; `false`, если клипа нет |

### Костевые треки в редакторе

У `AnimationComponent` в инспекторе есть секция **Bone Tracks** внутри
редактора клипа. Кость выбирается **по имени**, а не по номеру: редактор
находит скелет через привязку (`attachObject(objId, trackId, slot)`) — тот же
`trackId`, что и у трека, — и берёт имена из его `SkeletonComponent`. Без
привязки имён взять неоткуда, и остаётся ввод номера с подсказкой об этом.

| Кнопка | Что делает |
|---|---|
| **Add Bone Track** | заводит трек для выбранной кости |
| **Снять всю позу** | снимает текущую позу ВСЕХ костей скелета в кадры на заданное время |
| **Снять позу кости** | то же для одной кости |
| **Пустой кадр** | кадр с нулевой трансформацией |
| **Применить к скелету** | ставит скелет в позу выбранного кадра — так смотрят, что получилось |

Кадр на уже занятое время заменяется, а не добавляется вторым: интерполяция
от дубля не сломается (нулевой интервал отсекается), но повторное «снять позу»
плодило бы мёртвые кадры, из которых виден и правится только первый.
Правка времени кадра пересортировывает трек — интерполяция ждёт порядка.

Трек на кость за пределами скелета допустим (клип может быть шире рига) и при
проигрывании просто игнорируется; редактор помечает такой трек как `(?)`.

Позу костей правят прямо в инспекторе скелета — там список костей с
позицией, поворотом и масштабом. Обычный порядок работы: поставить позу,
выставить время, нажать «снять всю позу», повторить для следующего кадра.

`addBoneTrack`, `addBoneKeyframe` и `playClip` возвращают признак успеха —
опечатка в имени клипа иначе молча не делает ничего. `boneId` за пределами
скелета допустим (клип может быть шире рига) и просто игнорируется при
проигрывании. Идёт ли клип, показывает свойство `anim.isPlaying`.

## Формат карт

`.pemf` версии 16 хранит у объекта счётчик компонентов, а у каждого компонента —
тип, флаг включения, признак наличия локального сдвига и сам сдвиг. Номер 0
(старый `body`) больше не пишется: его заменили 11 (`mesh`) и 12 (`physics`).

Старые карты переводятся скриптом:

```bash
python3 other/pemf_migrate.py res/maps/          # вся папка, с бэкапами *.bak
python3 other/pemf_migrate.py res/maps/ --dry-run # только проверить
```

Миграция разбивает каждое старое тело на меш и — если тело действительно
участвовало в физике или коллизиях — физический компонент. Чистая декорация
физического компонента не получает.

Заодно в v15 у света начали сохраняться `cullingRadius`, `shadowNearPlane` и
`shadowFarPlane`: раньше их можно было выставить в редакторе, но при сохранении
карты они терялись.

В v16 у меша появились веса форм (shape keys). Старые карты после миграции
получают пустой список: вес по умолчанию нулевой, то есть геометрия остаётся
базовой и вид карты не меняется.