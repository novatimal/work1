// ScriptBindings.cpp
// Регистрация Lua API движка: Engine, Object, Physics, Graphics, RmlUi, ImGui и
// прочие таблицы. Вынесено из main.cpp отдельной единицей трансляции — иначе
// шаблоны sol2 вместе с остальным движком не влезают в память компилятора.
//
// Логика движка по-прежнему живёт в main.cpp; здесь только связывание с Lua.
#include "main.h"

Component* ComponentRef::get() const {
    if (!app || uid == 0) return nullptr;
    GameObject* go = app->getObjectById(objectId);
    if (!go) return nullptr;

    // Быстрый путь: индекс обычно ещё верен
    if (index >= 0 && index < static_cast<int>(go->components.size()) &&
        go->components[index].uid == uid) {
        return &go->components[index];
    }

    // Соседей удаляли — ищем по устойчивому uid и чиним подсказку
    for (size_t i = 0; i < go->components.size(); ++i) {
        if (go->components[i].uid == uid) {
            const_cast<ComponentRef*>(this)->index = static_cast<int>(i);
            return &go->components[i];
        }
    }
    return nullptr;   // компонент удалён
}

// Пересчитывает мировые трансформации всех компонентов объекта: нужно после
// правки локального сдвига, иначе мир обновится только на следующем кадре
void ComponentRef::refreshTransforms() const {
	if (!app) return;
	if (GameObject* go = app->getObjectById(objectId)) go->updateComponentTransforms();
}

// Регистрация типа ComponentRef вынесена отдельным методом: вместе с ObjectRef
// шаблоны sol2 не влезают в память компилятора.
void SceneEditorApp::bindComponentAPI() {
    sol::state& lua = m_scriptEngine->lua();

	// ============================================================
	// Тип ComponentRef — доступ к конкретному компоненту объекта
	// ============================================================
	auto ct = lua.new_usertype<ComponentRef>("Component", sol::no_constructor);

	// Свойства регистрируются по одному: так sol2 инстанцирует шаблоны
	// небольшими порциями и компилятору хватает памяти.
	ct["type"] = sol::property([](ComponentRef& ref) -> std::string {
		Component* comp = ref.get();
		return comp ? comp->getTypeName() : std::string();
	});

	ct["valid"] = sol::property([](ComponentRef& ref) -> bool { return ref.get() != nullptr; });

	ct["object"] = sol::property([this](ComponentRef& ref) -> sol::object {
		if (!ref.get()) return sol::nil;
		return sol::make_object(m_scriptEngine->lua(), ObjectRef{ref.objectId, this});
	});

	ct["enabled"] = sol::property(
		[](ComponentRef& ref) -> bool { Component* c = ref.get(); return c && c->enabled; },
		[](ComponentRef& ref, bool val) { if (Component* c = ref.get()) c->enabled = val; }
	);

	// Сдвиг компонента относительно объекта
	ct["localPosition"] = sol::property(
		[](ComponentRef& ref) -> LuaVec3 {
			Component* c = ref.get();
			return c ? LuaVec3(c->localPosition) : LuaVec3(0.0f, 0.0f, 0.0f);
		},
		[](ComponentRef& ref, const LuaVec3& val) {
			if (Component* c = ref.get()) {
				c->localPosition = val.toGlm();
				ref.refreshTransforms();
			}
		}
	);

	ct["localRotation"] = sol::property(
		[](ComponentRef& ref) -> LuaVec3 {
			Component* c = ref.get();
			return c ? LuaVec3(c->localRotation) : LuaVec3(0.0f, 0.0f, 0.0f);
		},
		[](ComponentRef& ref, const LuaVec3& val) {
			if (Component* c = ref.get()) {
				c->localRotation = val.toGlm();
				c->syncLocalQuatFromEuler();
				ref.refreshTransforms();
			}
		}
	);

	// Мировая позиция компонента — только чтение, пересчитывается движком
	ct["worldPosition"] = sol::property([](ComponentRef& ref) -> LuaVec3 {
		Component* c = ref.get();
		return c ? LuaVec3(c->worldPosition) : LuaVec3(0.0f, 0.0f, 0.0f);
	});

	// --- Свет ---
	ct["lightType"] = sol::property(
		[](ComponentRef& ref) -> int { auto* l = ref.as<LightComponent>(); return l ? static_cast<int>(l->type) : 0; },
		[](ComponentRef& ref, int val) { if (auto* l = ref.as<LightComponent>()) l->type = static_cast<LightType>(val); }
	);
	// Цвет есть у нескольких типов, и он разной размерности: у света vec3,
	// у частицы и связи vec4. Разбираем по типу компонента, как это делает
	// одноимённое плоское свойство объекта.
	ct["color"] = sol::property(
		[this](ComponentRef& ref) -> sol::object {
			sol::state& lua = m_scriptEngine->lua();
			if (auto* l  = ref.as<LightComponent>())          return sol::make_object(lua, LuaVec3(l->color));
			if (auto* sp = ref.as<SingleParticleComponent>()) return sol::make_object(lua, LuaVec4(sp->color));
			if (auto* cs = ref.as<ConstraintComponent>())     return sol::make_object(lua, LuaVec4(cs->color));
			return sol::object();
		},
		[](ComponentRef& ref, sol::object val) {
			if (auto* l = ref.as<LightComponent>()) {
				if (val.is<LuaVec3>()) l->color = val.as<LuaVec3>().toGlm();
			} else if (auto* sp = ref.as<SingleParticleComponent>()) {
				if (val.is<LuaVec4>()) sp->color = val.as<LuaVec4>().toGlm();
			} else if (auto* cs = ref.as<ConstraintComponent>()) {
				if (val.is<LuaVec4>()) cs->color = val.as<LuaVec4>().toGlm();
			}
		}
	);
	ct["intensity"] = sol::property(
		[](ComponentRef& ref) -> float { auto* l = ref.as<LightComponent>(); return l ? l->intensity : 0.0f; },
		[](ComponentRef& ref, float val) { if (auto* l = ref.as<LightComponent>()) l->intensity = val; }
	);
	ct["radius"] = sol::property(
		[](ComponentRef& ref) -> float { auto* l = ref.as<LightComponent>(); return l ? l->radius : 0.0f; },
		[](ComponentRef& ref, float val) { if (auto* l = ref.as<LightComponent>()) l->radius = val; }
	);
	ct["innerConeAngle"] = sol::property(
		[](ComponentRef& ref) -> float { auto* l = ref.as<LightComponent>(); return l ? l->innerConeAngle : 0.0f; },
		[](ComponentRef& ref, float val) { if (auto* l = ref.as<LightComponent>()) l->innerConeAngle = val; }
	);
	ct["outerConeAngle"] = sol::property(
		[](ComponentRef& ref) -> float { auto* l = ref.as<LightComponent>(); return l ? l->outerConeAngle : 0.0f; },
		[](ComponentRef& ref, float val) { if (auto* l = ref.as<LightComponent>()) l->outerConeAngle = val; }
	);
	ct["castShadow"] = sol::property(
		[](ComponentRef& ref) -> bool {
			if (auto* l = ref.as<LightComponent>()) return l->castShadow;
			if (auto* b = ref.as<MeshComponent>())  return b->castShadow();
			return false;
		},
		[](ComponentRef& ref, bool val) {
			if (auto* l = ref.as<LightComponent>()) l->castShadow = val;
			else if (auto* b = ref.as<MeshComponent>()) b->setCastShadow(val);
		}
	);

	// --- Звук ---
	ct["soundName"] = sol::property(
		[](ComponentRef& ref) -> std::string { auto* snd = ref.as<SoundComponent>(); return snd ? snd->name : std::string(); },
		[](ComponentRef& ref, const std::string& val) { if (auto* snd = ref.as<SoundComponent>()) snd->name = val; }
	);
	ct["volume"] = sol::property(
		[](ComponentRef& ref) -> float { auto* snd = ref.as<SoundComponent>(); return snd ? snd->volume : 0.0f; },
		[](ComponentRef& ref, float val) { if (auto* snd = ref.as<SoundComponent>()) snd->volume = val; }
	);
	ct["pitch"] = sol::property(
		[](ComponentRef& ref) -> float { auto* snd = ref.as<SoundComponent>(); return snd ? snd->pitch : 1.0f; },
		[](ComponentRef& ref, float val) { if (auto* snd = ref.as<SoundComponent>()) snd->pitch = val; }
	);
	ct["loop"] = sol::property(
		[](ComponentRef& ref) -> bool { auto* snd = ref.as<SoundComponent>(); return snd && snd->loop; },
		[](ComponentRef& ref, bool val) { if (auto* snd = ref.as<SoundComponent>()) snd->loop = val; }
	);
	ct["is3DSound"] = sol::property(
		[](ComponentRef& ref) -> bool { auto* snd = ref.as<SoundComponent>(); return snd && snd->type == 1; },
		[](ComponentRef& ref, bool val) { if (auto* snd = ref.as<SoundComponent>()) snd->type = val ? 1 : 0; }
	);
	ct["minDistance"] = sol::property(
		[](ComponentRef& ref) -> float { auto* snd = ref.as<SoundComponent>(); return snd ? snd->minDistance : 0.0f; },
		[](ComponentRef& ref, float val) { if (auto* snd = ref.as<SoundComponent>()) snd->minDistance = val; }
	);
	ct["maxDistance"] = sol::property(
		[](ComponentRef& ref) -> float { auto* snd = ref.as<SoundComponent>(); return snd ? snd->maxDistance : 0.0f; },
		[](ComponentRef& ref, float val) { if (auto* snd = ref.as<SoundComponent>()) snd->maxDistance = val; }
	);

	// Без них добавленный через addComponent звук невозможно было запустить:
	// имя и громкость выставить давали, а включить — нет.
	// 3D-звук звучит из МИРОВОЙ позиции компонента, а она учитывает
	// локальный сдвиг, — поэтому динамик на объекте слышно оттуда, где он стоит.
	ct["play"] = [this](ComponentRef& ref) {
		auto* snd = ref.as<SoundComponent>();
		if (!snd || snd->name.empty() || !m_audio) return false;
		Component* comp = ref.get();
		if (!comp) return false;

		if (snd->instanceId != 0) m_audio->stop(snd->instanceId);
		if (snd->type == 1) {
			ref.refreshTransforms();   // мировая позиция могла устареть
			snd->instanceId = m_audio->play(snd->name, snd->volume, snd->pitch, snd->loop,
			                                &comp->worldPosition, snd->minDistance,
			                                snd->maxDistance, snd->rolloff);
			snd->prevPosition = comp->worldPosition;
		} else {
			snd->instanceId = m_audio->play(snd->name, snd->volume, snd->pitch, snd->loop,
			                                nullptr, 0.0f, 0.0f, 0.0f);
		}
		snd->isPlaying = true;
		return true;
	};
	ct["stop"] = [this](ComponentRef& ref) {
		auto* snd = ref.as<SoundComponent>();
		if (!snd) return false;
		if (snd->instanceId != 0 && m_audio) m_audio->stop(snd->instanceId);
		snd->instanceId = 0;
		snd->isPlaying = false;
		return true;
	};

	// --- Одиночная частица ---
	// Раньше компонент частицы можно было только создать: ни текстуру задать,
	// ни размер. Имена свойств те же, что у плоских свойств объекта.
	ct["texture"] = sol::property(
		[](ComponentRef& ref) -> std::string {
			auto* sp = ref.as<SingleParticleComponent>(); return sp ? sp->textureName : std::string();
		},
		[this](ComponentRef& ref, const std::string& name) {
			if (auto* sp = ref.as<SingleParticleComponent>()) {
				sp->textureName = name;
				sp->textureHandle = graphics->getTextureHandleByName(name);
			}
		}
	);
	ct["size"] = sol::property(
		[](ComponentRef& ref) -> float { auto* sp = ref.as<SingleParticleComponent>(); return sp ? sp->size : 0.0f; },
		[](ComponentRef& ref, float val) { if (auto* sp = ref.as<SingleParticleComponent>()) sp->size = val; }
	);
	ct["brightness"] = sol::property(
		[](ComponentRef& ref) -> float { auto* sp = ref.as<SingleParticleComponent>(); return sp ? sp->brightness : 0.0f; },
		[](ComponentRef& ref, float val) { if (auto* sp = ref.as<SingleParticleComponent>()) sp->brightness = val; }
	);
	ct["opacity"] = sol::property(
		[](ComponentRef& ref) -> float { auto* sp = ref.as<SingleParticleComponent>(); return sp ? sp->opacity : 0.0f; },
		[](ComponentRef& ref, float val) { if (auto* sp = ref.as<SingleParticleComponent>()) sp->opacity = val; }
	);
	ct["blendMode"] = sol::property(
		[](ComponentRef& ref) -> int {
			auto* sp = ref.as<SingleParticleComponent>();
			return sp ? static_cast<int>(sp->blendMode) : 0;
		},
		[](ComponentRef& ref, int val) {
			if (auto* sp = ref.as<SingleParticleComponent>())
				sp->blendMode = static_cast<ParticleBlendMode>(val);
		}
	);

	// --- Физическое тело ---
	ct["mass"] = sol::property(
		[](ComponentRef& ref) -> float { auto* b = ref.as<PhysicsBodyComponent>(); return b ? b->mass : 0.0f; },
		[](ComponentRef& ref, float val) { if (auto* b = ref.as<PhysicsBodyComponent>()) b->mass = val; }
	);
	ct["physicsEnabled"] = sol::property(
		[](ComponentRef& ref) -> bool { auto* b = ref.as<PhysicsBodyComponent>(); return b && b->physicsEnabled; },
		[](ComponentRef& ref, bool val) { if (auto* b = ref.as<PhysicsBodyComponent>()) b->physicsEnabled = val; }
	);
	ct["collisionEnabled"] = sol::property(
		[](ComponentRef& ref) -> bool { auto* b = ref.as<PhysicsBodyComponent>(); return b && b->collisionEnabled; },
		[](ComponentRef& ref, bool val) { if (auto* b = ref.as<PhysicsBodyComponent>()) b->collisionEnabled = val; }
	);
	ct["shapeType"] = sol::property(
		[](ComponentRef& ref) -> int { auto* b = ref.as<PhysicsBodyComponent>(); return b ? b->shapeType : 0; },
		[](ComponentRef& ref, int val) { if (auto* b = ref.as<PhysicsBodyComponent>()) b->shapeType = val; }
	);

	// --- Меш ---
	ct["paintColor"] = sol::property(
		[](ComponentRef& ref) -> LuaVec4 { auto* b = ref.as<MeshComponent>(); return b ? LuaVec4(b->paintColor) : LuaVec4(1.0f, 1.0f, 1.0f, 1.0f); },
		[](ComponentRef& ref, const LuaVec4& val) { if (auto* b = ref.as<MeshComponent>()) b->paintColor = val.toGlm(); }
	);
	ct["mesh"] = sol::property(
		[this](ComponentRef& ref) -> std::string {
			auto* b = ref.as<MeshComponent>();
			return b ? getMeshNameByHandle(b->renderable) : std::string();
		},
		[this](ComponentRef& ref, const std::string& val) {
			if (auto* b = ref.as<MeshComponent>()) b->renderable = getMeshHandleByName(val);
		}
	);
	ct["material"] = sol::property(
		[this](ComponentRef& ref) -> std::string {
			auto* b = ref.as<MeshComponent>();
			return b ? getMaterialNameByHandle(b->materialHandle) : std::string();
		},
		[this](ComponentRef& ref, const std::string& val) {
			if (auto* b = ref.as<MeshComponent>()) b->materialHandle = getMaterialHandleByName(val);
		}
	);
	ct["isSkinnedMesh"] = sol::property(
		[](ComponentRef& ref) -> bool { auto* b = ref.as<MeshComponent>(); return b && b->isSkinnedMesh; },
		[](ComponentRef& ref, bool val) { if (auto* b = ref.as<MeshComponent>()) b->isSkinnedMesh = val; }
	);
	ct["skeletonId"] = sol::property(
		[](ComponentRef& ref) -> uint32_t { auto* b = ref.as<MeshComponent>(); return b ? b->skeletonId : 0u; },
		[](ComponentRef& ref, uint32_t val) { if (auto* b = ref.as<MeshComponent>()) b->skeletonId = val; }
	);

	// --- Партиклы ---
	ct["rate"] = sol::property(
		[](ComponentRef& ref) -> float { auto* e = ref.as<ParticleEmitterComponent>(); return e ? e->rate : 0.0f; },
		[](ComponentRef& ref, float val) { if (auto* e = ref.as<ParticleEmitterComponent>()) e->rate = val; }
	);
	ct["isPlaying"] = sol::property(
		[](ComponentRef& ref) -> bool {
			if (auto* e = ref.as<ParticleEmitterComponent>()) return e->isPlaying;
			if (auto* snd = ref.as<SoundComponent>()) return snd->isPlaying;
			return false;
		},
		[](ComponentRef& ref, bool val) { if (auto* e = ref.as<ParticleEmitterComponent>()) e->isPlaying = val; }
	);
	ct["maxParticles"] = sol::property(
		[](ComponentRef& ref) -> int { auto* e = ref.as<ParticleEmitterComponent>(); return e ? e->maxParticles : 0; },
		[](ComponentRef& ref, int val) { if (auto* e = ref.as<ParticleEmitterComponent>()) e->maxParticles = val; }
	);

	// --- Камера ---
	ct["fov"] = sol::property(
		[](ComponentRef& ref) -> float { auto* cam = ref.as<CameraComponent>(); return cam ? cam->fov : 0.0f; },
		[](ComponentRef& ref, float val) { if (auto* cam = ref.as<CameraComponent>()) cam->fov = val; }
	);
	ct["renderTextureSlot"] = sol::property(
		[](ComponentRef& ref) -> int { auto* cam = ref.as<CameraComponent>(); return cam ? cam->renderTextureSlot : 0; },
		[](ComponentRef& ref, int val) {
			// Клампим так же, как ObjectRef: слот за границей массива целей
			// молча не работал, но при этом сохранялся в .pemf
			if (auto* cam = ref.as<CameraComponent>())
				cam->renderTextureSlot = glm::clamp(val, 0, GraphicsEngineGL::MAX_CAMERA_TARGETS - 1);
		}
	);

}

GameObject* ObjectRef::get() const {
    return app ? app->getObjectById(id) : nullptr;
}


void SceneEditorApp::registerImGuiAPI() {
    sol::table imgui = m_scriptEngine->lua().create_named_table("ImGui");

    // =========================================================================
    // ОКНА
    // =========================================================================
	imgui["loadFont"] = [this](const std::string& path, float fontSize, bool pixelSnap) -> bool {
		if (!isPathSafe(path)) {
			m_scriptEngine->log("ImGui.loadFont: Access denied to path: " + path,
			                    ConsoleLogEntry::Error);
			return false;
		}

		ImGuiIO& io = ImGui::GetIO();

		// 1. Настраиваем конфигурацию шрифта
		ImFontConfig font_cfg;
		font_cfg.OversampleH = 1;
		font_cfg.OversampleV = 1;
		font_cfg.PixelSnapH = pixelSnap; // Жесткая привязка к пиксельной сетке (четкость как в Win98!)

		// 2. Загружаем шрифт через VFS — он видит и паки, и обычную ФС

		ImFont* my_font = addFontFromVfs(
			path, fontSize, &font_cfg,
			io.Fonts->GetGlyphRangesCyrillic()   // без кириллического диапазона русский текст в UI пропадает
		);

		// (Опционально) Проверка, загрузился ли шрифт
		return my_font != nullptr;
	};

    imgui["Begin"] = [](const std::string& name, sol::optional<bool> open) -> std::tuple<bool, bool> {
        bool isOpen = open.value_or(true);
        bool collapsed = ImGui::Begin(name.c_str(), &isOpen);
        return {isOpen, collapsed}; // Возвращаем состояние open (чтобы знать, нажали ли крестик) и видимость
    };
    imgui["End"] = []() { ImGui::End(); };

    imgui["SetNextWindowSize"] = [](float w, float h, sol::optional<int> cond) {
        ImGui::SetNextWindowSize(ImVec2(w, h), cond.value_or(ImGuiCond_FirstUseEver));
    };
    imgui["SetNextWindowPos"] = [](float x, float y, sol::optional<int> cond) {
        ImGui::SetNextWindowPos(ImVec2(x, y), cond.value_or(ImGuiCond_FirstUseEver));
    };

    // =========================================================================
    // ТЕКСТ И ОТСТУПЫ
    // =========================================================================
    imgui["Text"] = [](const std::string& text) { ImGui::Text("%s", text.c_str()); };
    imgui["TextColored"] = [](float r, float g, float b, const std::string& text) {
        ImGui::TextColored(ImVec4(r, g, b, 1.0f), "%s", text.c_str());
    };
    imgui["Separator"] = []() { ImGui::Separator(); };
    imgui["SameLine"] = []() { ImGui::SameLine(); };
    imgui["NewLine"] = []() { ImGui::NewLine(); };
    imgui["Spacing"] = []() { ImGui::Spacing(); };
    imgui["Indent"] = []() { ImGui::Indent(); };
    imgui["Unindent"] = []() { ImGui::Unindent(); };

    // =========================================================================
    // КНОПКИ И КЛИКАБЕЛЬНЫЕ ЭЛЕМЕНТЫ
    // =========================================================================
    imgui["Button"] = [](const std::string& label, sol::optional<float> w, sol::optional<float> h) -> bool {
        return ImGui::Button(label.c_str(), ImVec2(w.value_or(0), h.value_or(0)));
    };
    imgui["SmallButton"] = [](const std::string& label) -> bool {
        return ImGui::SmallButton(label.c_str());
    };
    imgui["Selectable"] = [](const std::string& label, bool selected) -> bool {
        return ImGui::Selectable(label.c_str(), selected);
    };

    // =========================================================================
    // ВВОД ДАННЫХ (Возвращают [изменилось_ли, новое_значение])
    // =========================================================================
    imgui["Checkbox"] = [](const std::string& label, bool v) -> std::tuple<bool, bool> {
        bool changed = ImGui::Checkbox(label.c_str(), &v);
        return {changed, v};
    };

    imgui["SliderFloat"] = [](const std::string& label, float v, float min, float max) -> std::tuple<bool, float> {
        bool changed = ImGui::SliderFloat(label.c_str(), &v, min, max, "%.3f", ImGuiSliderFlags_None);
        return {changed, v};
    };

    imgui["SliderInt"] = [](const std::string& label, int v, int min, int max) -> std::tuple<bool, int> {
        bool changed = ImGui::SliderInt(label.c_str(), &v, min, max);
        return {changed, v};
    };

    imgui["DragFloat"] = [](const std::string& label, float v, sol::optional<float> speed, sol::optional<float> min, sol::optional<float> max) -> std::tuple<bool, float> {
        bool changed = ImGui::DragFloat(label.c_str(), &v, speed.value_or(1.0f), min.value_or(0.0f), max.value_or(0.0f), "%.3f");
        return {changed, v};
    };

    imgui["DragInt"] = [](const std::string& label, int v, sol::optional<float> speed, sol::optional<int> min, sol::optional<int> max) -> std::tuple<bool, int> {
		(void)max;
        bool changed = ImGui::DragInt(label.c_str(), &v, speed.value_or(1.0f), min.value_or(0), min.value_or(0));
        return {changed, v};
    };

    imgui["InputText"] = [](const std::string& label, const std::string& str) -> std::tuple<bool, std::string> {
        static char buffer[1024];
        strncpy(buffer, str.c_str(), sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';
        bool changed = ImGui::InputText(label.c_str(), buffer, sizeof(buffer));
        return {changed, std::string(buffer)};
    };

    imgui["InputFloat"] = [](const std::string& label, float v) -> std::tuple<bool, float> {
        bool changed = ImGui::InputFloat(label.c_str(), &v);
        return {changed, v};
    };

    imgui["ColorEdit3"] = [](const std::string& label, LuaVec3 col) -> std::tuple<bool, LuaVec3> {
        bool changed = ImGui::ColorEdit3(label.c_str(), &col.x);
        return {changed, col};
    };

    imgui["ColorEdit4"] = [](const std::string& label, LuaVec4 col) -> std::tuple<bool, LuaVec4> {
        bool changed = ImGui::ColorEdit4(label.c_str(), &col.x);
        return {changed, col};
    };

    // =========================================================================
    // ВЫПАДАЮЩИЕ СПИСКИ (COMBO)
    // =========================================================================
    imgui["BeginCombo"] = [](const std::string& label, const std::string& preview) -> bool {
        return ImGui::BeginCombo(label.c_str(), preview.c_str());
    };
    imgui["EndCombo"] = []() { ImGui::EndCombo(); };

    // =========================================================================
    // ДЕРЕВЬЯ (TREES) - ДЛЯ ИЕРАРХИИ СЦЕНЫ
    // =========================================================================
    imgui["TreeNode"] = [](const std::string& label) -> bool {
        return ImGui::TreeNode(label.c_str());
    };
    imgui["TreePop"] = []() { ImGui::TreePop(); };

    imgui["CollapsingHeader"] = [](const std::string& label) -> bool {
        return ImGui::CollapsingHeader(label.c_str());
    };

    // =========================================================================
    // ТАБЛИЦЫ / КОЛОНКИ - ДЛЯ ИНСПЕКТОРА
    // =========================================================================
    imgui["Columns"] = [](int count, sol::optional<bool> border) {
        ImGui::Columns(count, nullptr, border.value_or(true));
    };
    imgui["NextColumn"] = []() { ImGui::NextColumn(); };

    // =========================================================================
    // ТОЛСТЫЕ (TOOLTIPS) И СТИЛЬ
    // =========================================================================
    imgui["SetTooltip"] = [](const std::string& text) { ImGui::SetTooltip("%s", text.c_str()); };
    imgui["IsItemHovered"] = []() -> bool { return ImGui::IsItemHovered(); };
    imgui["PushItemWidth"] = [](float w) { ImGui::PushItemWidth(w); };
    imgui["PopItemWidth"] = []() { ImGui::PopItemWidth(); };

	imgui["IsAnyWindowHovered"] = []() -> bool { return ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow); };
	imgui["IsAnyItemActive"] = []() -> bool { return ImGui::IsAnyItemActive(); };

	// =========================================================================
    // GIZMO API (ImGuizmo)
    // =========================================================================
    sol::table gizmo = m_scriptEngine->lua().create_named_table("Gizmo");

	gizmo["setOperation"] = [this](int op) { m_gizmoOperation = op; };
	gizmo["setMode"] = [this](int mode) { m_gizmoMode = mode; };
	gizmo["getOperation"] = [this]() -> int { return m_gizmoOperation; };
	gizmo["getMode"] = [this]() -> int { return m_gizmoMode; };

    gizmo["isUsing"] = []() -> bool {
        return ImGuizmo::IsUsing();
    };

    gizmo["isOver"] = []() -> bool {
        return ImGuizmo::IsOver();
    };

    gizmo["snapTranslate"] = sol::property(
        [this]() -> float { return m_gizmoSnapTranslate; },
        [this](float val) { m_gizmoSnapTranslate = val; }
    );

    gizmo["snapRotate"] = sol::property(
        [this]() -> float { return m_gizmoSnapRotate; },
        [this](float val) { m_gizmoSnapRotate = val; }
    );

    gizmo["snapScale"] = sol::property(
        [this]() -> float { return m_gizmoSnapScale; },
        [this](float val) { m_gizmoSnapScale = val; }
    );
}


void SceneEditorApp::bindScriptingAPI() {
    sol::state& lua = m_scriptEngine->lua();

    sol::table jsonApi = m_scriptEngine->lua().create_named_table("Json");

    // 1. Функция декодирования (JSON string -> Lua table)
    jsonApi["decode"] = [this](const std::string& jsonString) -> sol::object {
        try {
            nlohmann::json j = nlohmann::json::parse(jsonString);

            // Рекурсивная функция для конвертации nlohmann::json в sol::object
            std::function<sol::object(const nlohmann::json&)> toSol = [&](const nlohmann::json& val) -> sol::object {
                sol::state_view lua = m_scriptEngine->lua();
                if (val.is_null()) return sol::nil;
                if (val.is_boolean()) return sol::make_object(lua, val.get<bool>());
                if (val.is_number_integer()) return sol::make_object(lua, val.get<int>());
                if (val.is_number_float()) return sol::make_object(lua, val.get<double>());
                if (val.is_string()) return sol::make_object(lua, val.get<std::string>());
                if (val.is_array()) {
                    sol::table t = lua.create_table();
                    int idx = 1;
                    for (const auto& item : val) {
                        t[idx++] = toSol(item);
                    }
                    return t;
                }
                if (val.is_object()) {
                    sol::table t = lua.create_table();
                    for (auto& [key, value] : val.items()) {
                        t[key] = toSol(value);
                    }
                    return t;
                }
                return sol::nil;
            };

            return toSol(j);
        } catch (const nlohmann::json::parse_error& e) {
            m_scriptEngine->log("JSON Parse Error: " + std::string(e.what()), ConsoleLogEntry::Error);
            return sol::nil;
        }
    };

    // 2. Функция кодирования (Lua table -> JSON string)
    jsonApi["encode"] = [this](sol::table luaTable) -> std::string {
        try {
            // Рекурсивная функция для безопасной конвертации sol::object в nlohmann::json
            std::function<nlohmann::json(sol::object)> toSolJson = [&](sol::object val) -> nlohmann::json {
                if (!val.valid() || val.is<sol::nil_t>()) return nullptr;
                if (val.is<bool>()) return val.as<bool>();
                if (val.is<double>()) return val.as<double>();
                if (val.is<int>()) return val.as<int>();
                if (val.is<std::string>()) return val.as<std::string>();

                if (val.is<sol::table>()) {
                    sol::table tbl = val.as<sol::table>();

                    // Проверяем, является ли таблица чистым массивом (все ключи числовые)
                    bool isArray = true;
                    size_t count = 0;
                    for (auto pair : tbl) {
                        count++;
                        if (!pair.first.is<double>() && !pair.first.is<int>()) {
                            isArray = false;
                            break;
                        }
                    }

                    // Если это похоже на массив, собираем его строго по индексам 1..N (как в Lua)
                    if (isArray && count > 0) {
                        nlohmann::json jArray = nlohmann::json::array();
                        size_t len = tbl.size(); // Вызывает lua_rawlen, самый надёжный способ
                        for (size_t i = 1; i <= len; ++i) {
                            sol::object item = tbl[i];
                            if (item.valid() && !item.is<sol::nil_t>()) {
                                jArray.push_back(toSolJson(item));
                            } else {
                                jArray.push_back(nullptr); // Сохраняем дырки в массиве как null
                            }
                        }
                        return jArray;
                    }
                    // Иначе собираем как JSON-объект (словарь)
                    else {
                        nlohmann::json jObj = nlohmann::json::object();
                        for (auto pair : tbl) {
                            std::string key = "key";
                            if (pair.first.is<std::string>()) {
                                key = pair.first.as<std::string>();
                            } else if (pair.first.is<double>() || pair.first.is<int>()) {
                                key = std::to_string(pair.first.as<double>());
                            }
                            jObj[key] = toSolJson(pair.second);
                        }
                        return jObj;
                    }
                }
                return nullptr;
            };

            nlohmann::json j = toSolJson(luaTable);
            return j.dump(4); // 4 пробела для красивого форматирования (pretty print)
        } catch (const std::exception& e) {
            m_scriptEngine->log("JSON Encode Error: " + std::string(e.what()), ConsoleLogEntry::Error);
            return "{}";
        }
    };

    // 3. Удобная функция для загрузки JSON прямо из файла
    jsonApi["loadFile"] = [this](const std::string& filepath) -> sol::object {
        auto fileText = vfs::readText(filepath);
        if (!fileText) {
            m_scriptEngine->log("Json.loadFile: Cannot open file: " + filepath, ConsoleLogEntry::Error);
            return sol::nil;
        }
        try {
            nlohmann::json j = nlohmann::json::parse(*fileText);

            std::function<sol::object(const nlohmann::json&)> toSol = [&](const nlohmann::json& val) -> sol::object {
                sol::state_view lua = m_scriptEngine->lua();
                if (val.is_null()) return sol::nil;
                if (val.is_boolean()) return sol::make_object(lua, val.get<bool>());
                if (val.is_number_integer()) return sol::make_object(lua, val.get<int>());
                if (val.is_number_float()) return sol::make_object(lua, val.get<double>());
                if (val.is_string()) return sol::make_object(lua, val.get<std::string>());
                if (val.is_array()) {
                    sol::table t = lua.create_table();
                    int idx = 1;
                    for (const auto& item : val) t[idx++] = toSol(item);
                    return t;
                }
                if (val.is_object()) {
                    sol::table t = lua.create_table();
                    for (auto& [key, value] : val.items()) t[key] = toSol(value);
                    return t;
                }
                return sol::nil;
            };
            return toSol(j);
        } catch (const std::exception& e) {
            m_scriptEngine->log("Json.loadFile Error: " + std::string(e.what()), ConsoleLogEntry::Error);
            return sol::nil;
        }
    };

    // ===== Engine API =====
    sol::table engine = lua.create_named_table("Engine");
    engine["debugui"] = [this](bool val) { m_drawDebugUI = val; };
    // Именно функция, а не свойство: Engine — обычная таблица Lua, и sol::property
    // на ней не работает — чтение вернуло бы функцию, а присваивание просто
    // затёрло бы поле, ничего не переключив.
    engine["debugUI"] = [this](bool val) { m_drawDebugUI = val; };
    engine["isDebugUI"] = [this]() -> bool { return m_drawDebugUI; };

    // Объект, выбранный в редакторе: именно его показывает инспектор.
    // Принимает и ObjectRef, и числовой id; 0 — снять выделение.
    engine["selectObject"] = [this](sol::object arg) -> bool {
        uint32_t id = 0;
        if (arg.is<ObjectRef>())      id = arg.as<ObjectRef>().id;
        else if (arg.is<uint32_t>())  id = arg.as<uint32_t>();
        else if (arg.is<double>())    id = static_cast<uint32_t>(arg.as<double>());
        else if (arg.valid() && arg != sol::nil) return false;

        if (id != 0 && !getObjectById(id)) return false;   // объекта нет
        selectedObjectId = id;
        return true;
    };
    // Окна редактора по имени. Нужно, чтобы скриптом собрать нужную раскладку
    // (в том числе для визуальных тестов), не тыкая галки в меню View.
    auto windowFlag = [this](const std::string& name) -> bool* {
        if (name == "scene"     || name == "Scene Objects")     return &m_showSceneWindow;
        if (name == "graphics"  || name == "Graphics Settings") return &m_showGraphicsSettings;
        if (name == "stats"     || name == "Statistics")        return &m_showStatsWindow;
        if (name == "resources" || name == "Resource Manager")  return &m_showResourceManager;
        if (name == "console"   || name == "Lua Console")       return &m_showConsole;
        return nullptr;
    };
    engine["showWindow"] = [this, windowFlag](const std::string& name, bool visible) -> bool {
        bool* flag = windowFlag(name);
        if (!flag) {
            m_scriptEngine->log("showWindow: неизвестное окно '" + name + "'", ConsoleLogEntry::Error);
            return false;
        }
        *flag = visible;
        return true;
    };
    engine["isWindowShown"] = [windowFlag](const std::string& name) -> bool {
        const bool* flag = windowFlag(name);
        return flag && *flag;
    };

    engine["getSelectedObject"] = [this]() -> sol::object {
        if (selectedObjectId == 0 || !getObjectById(selectedObjectId)) return sol::nil;
        return sol::make_object(m_scriptEngine->lua(), ObjectRef{selectedObjectId, this});
    };
    engine["start"] = [this]() {
        m_scriptEngine->setRunning(true);
        m_scriptEngine->onSceneStart();
    };
    engine["stop"] = [this]() {
        m_scriptEngine->setRunning(false);
        m_scriptEngine->onSceneStop();
    };
    // Закрыть окно и выйти из движка
    engine["quit"] = [this]() { glfwSetWindowShouldClose(window, GLFW_TRUE); };

    // Снимок кадра в PNG. Сам кадр ещё не нарисован, поэтому запоминаем путь
    // и пишем в конце кадра. Нужен прежде всего для визуальных тестов.
    engine["screenshot"] = [this](const std::string& path) -> bool {
        if (!isPathSafe(path)) {
            m_scriptEngine->log("Engine.screenshot: Access denied to path: " + path, ConsoleLogEntry::Error);
            return false;
        }
        m_pendingScreenshot = path;
        return true;
    };
    engine["loadScene"] = [this](const std::string& name) {
        if (!isPathSafe(name)) {
            m_scriptEngine->log("Engine.loadScene: Access denied to path: " + name, ConsoleLogEntry::Error);
            return;
        }
        loadScene(name);
    };
    engine["saveScene"] = [this](const std::string& name) {
        if (!isPathSafe(name)) {
            m_scriptEngine->log("Engine.saveScene: Access denied to path: " + name, ConsoleLogEntry::Error);
            return;
        }
        saveScene(name);
    };

	engine["compileScriptFromFileToFile"] = [this](const std::string& src, const std::string& dst, const int debugFlag = 1) -> bool {
        if (!isPathSafe(src) || !isPathSafe(dst)) {
            m_scriptEngine->log("Engine.compileScriptFromFileToFile: Access denied", ConsoleLogEntry::Error);
            return false;
        }
        return m_scriptEngine->compileScript(src, dst, debugFlag);
    };

	// --- Паки ресурсов (.pep) ---
	// Пути проверяются так же, как в readFile/writeFile: скрипт не должен
	// доставать файлы за пределами папки игры
	engine["mountPack"] = [this](const std::string& path) -> bool {
		if (!isPathSafe(path)) {
			m_scriptEngine->log("Engine.mountPack: Access denied to path: " + path, ConsoleLogEntry::Error);
			return false;
		}
		return vfs::mountPack(path);
	};
	engine["mountPackDirectory"] = [this](const std::string& dir) -> int {
		if (!isPathSafe(dir)) {
			m_scriptEngine->log("Engine.mountPackDirectory: Access denied to path: " + dir, ConsoleLogEntry::Error);
			return 0;
		}
		return vfs::mountDirectory(dir);
	};
	engine["packCount"] = []() -> int {
		return static_cast<int>(vfs::packCount());
	};
	engine["fileExists"] = [](const std::string& path) -> bool {
		return SceneEditorApp::isPathSafe(path) && vfs::exists(path);
	};

	engine["readFile"] = [this](const std::string& path) -> sol::object {
		if (!isPathSafe(path)) {
			m_scriptEngine->log("Engine.readFile: Access denied to path: " + path, ConsoleLogEntry::Error);
			return sol::nil;
		}
		auto content = vfs::readText(path);
		if (!content) return sol::nil;
		return sol::make_object(m_scriptEngine->lua(), *content);
	};

	engine["writeFile"] = [this](const std::string& path, const std::string& content) -> bool {
		if (!isPathSafe(path)) {
			m_scriptEngine->log("Engine.writeFile: Access denied to path: " + path, ConsoleLogEntry::Error);
			return false;
		}
		std::ofstream file(path, std::ios::binary);
		if (!file.is_open()) return false;
		file << content;
		return true;
	};

	engine["appendFile"] = [this](const std::string& path, const std::string& content) -> bool {
		if (!isPathSafe(path)) {
			m_scriptEngine->log("Engine.appendFile: Access denied to path: " + path, ConsoleLogEntry::Error);
			return false;
		}
		std::ofstream file(path, std::ios::app | std::ios::binary);
		if (!file.is_open()) return false;
		file << content;
		return true;
	};

    bindObjectAPI();
    registerImGuiAPI();

    // ===== SOUND API =====
    sol::table sound = lua.create_named_table("Sound");

    lua.new_enum<AudioEnvironment>("AudioEnvironment", {
        { "Default",   AudioEnvironment::Default },
        { "OpenSpace", AudioEnvironment::OpenSpace },
        { "Hangar",    AudioEnvironment::Hangar },
        { "Tunnel",    AudioEnvironment::Tunnel }
    });

    sol::table env_table = lua["AudioEnvironment"];
    sound["Default"]   = env_table["Default"];
    sound["OpenSpace"] = env_table["OpenSpace"];
    sound["Hangar"]    = env_table["Hangar"];
    sound["Tunnel"]    = env_table["Tunnel"];

    sound["setEnvironment"] = [this](int env) {
        if (m_audio) m_audio->setEnvironment(static_cast<AudioEnvironment>(env));
    };
    sound["setEchoParameters"] = [this](float delay, float decay) {
        if (m_audio) m_audio->setEchoParameters(delay, decay);
    };
	sound["setDopplerFactor"] = [this](float factor) {
		if (m_audio) m_audio->setDopplerFactor(factor);
	};
    sound["setEnvironmentConfig"] = [this](int env, float rolloff, bool useDistLpf, bool useFlatLpf, float startDist, float maxDist, float intensity) {
        if (m_audio) {
            EnvConfig cfg;
            cfg.rolloff = rolloff;
            cfg.useDistanceLpf = useDistLpf;
            cfg.useFlatLpf = useFlatLpf;
            cfg.lpfStartDist = startDist;
            cfg.lpfMaxDist = maxDist;
            cfg.lpfIntensity = intensity;
            m_audio->setEnvironmentConfig(static_cast<AudioEnvironment>(env), cfg);
        }
    };
    sound["play2D"] = [this](const std::string& name, sol::optional<float> volume, sol::optional<float> pitch, sol::optional<bool> loop) -> SoundInstanceId {
        if (!m_audio) return 0;
        return m_audio->play(name, volume.value_or(1.0f), pitch.value_or(1.0f), loop.value_or(false), nullptr);
    };
    sound["play3D"] = [this](const std::string& name, const LuaVec3& position, sol::optional<float> volume, sol::optional<float> pitch, sol::optional<bool> loop) -> SoundInstanceId {
        if (!m_audio) return 0;
        glm::vec3 pos = position.toGlm();
        return m_audio->play(name, volume.value_or(1.0f), pitch.value_or(1.0f), loop.value_or(false), &pos);
    };
    sound["stop"] = [this](SoundInstanceId id) {
        if (m_audio) m_audio->stop(id);
    };
    sound["stopAll"] = [this]() {
        if (m_audio) m_audio->stopAll();
    };
    sound["setVolume"] = [this](SoundInstanceId id, float vol) {
        if (m_audio) m_audio->setVolume(id, vol);
    };
    sound["setPitch"] = [this](SoundInstanceId id, float pitch) {
        if (m_audio) m_audio->setPitch(id, pitch);
    };
    sound["setPosition"] = [this](SoundInstanceId id, const LuaVec3& pos) {
        if (m_audio) m_audio->setPosition(id, pos.toGlm());
    };
    sound["isPlaying"] = [this](SoundInstanceId id) -> bool {
        return m_audio ? m_audio->isPlaying(id) : false;
    };
    sound["setMasterVolume"] = [this](float vol) {
        if (m_audio) m_audio->setMasterVolume(vol);
    };
    sound["getMasterVolume"] = [this]() -> float {
        return m_audio ? m_audio->getMasterVolume() : 0.0f;
    };
    sound["playMusic"] = [this](const std::string& filepath, sol::optional<float> volume, sol::optional<bool> loop) {
        if (m_audio) m_audio->playMusic(filepath, volume.value_or(1.0f), loop.value_or(true));
    };
    sound["stopMusic"] = [this]() {
        if (m_audio) m_audio->stopMusic();
    };
    sound["setMusicVolume"] = [this](float vol) {
        if (m_audio) m_audio->setMusicVolume(vol);
    };
    sound["isMusicPlaying"] = [this]() -> bool {
        return m_audio ? m_audio->isMusicPlaying() : false;
    };
    sound["preload"] = [this](const std::string& name) -> float {
        if (!m_audio) return 0.0f;
        return m_audio->preloadSound(name);
    };
    sound["unload"] = [this](const std::string& name) {
        if (m_audio) m_audio->unloadSound(name);
    };
    sound["getDuration"] = [this](const std::string& name) -> float {
        if (!m_audio) return 0.0f;
        return m_audio->getSoundDuration(name);
    };

    // ===== CAMERA API =====
    sol::table cam = lua["Camera"];
	cam["setInputMode"] = [this](int mode) { setInputMode(static_cast<InputMode>(mode)); };
	cam["getInputMode"] = [this]() -> int { return static_cast<int>(currentInputMode); };
    cam["getPosition"] = [this]() -> LuaVec3 { return LuaVec3(camera.pos); };
    cam["setPosition"] = [this](const LuaVec3& pos) { camera.pos = pos.toGlm(); };
    cam["getFront"] = [this]() -> LuaVec3 { return LuaVec3(camera.front); };
    cam["setFront"] = [this](const LuaVec3& front) { camera.front = glm::normalize(front.toGlm()); };
    cam["lookAt"] = [this](const LuaVec3& target) {
        glm::vec3 dir = target.toGlm() - camera.pos;
        if (glm::length(dir) > 0.001f) camera.front = glm::normalize(dir);
    };
    cam["getFOV"] = [this]() -> float { return camera.fov; };
    cam["setFOV"] = [this](float fov) { camera.fov = glm::clamp(fov, 10.0f, 120.0f); };
    cam["setFarPlane"] = [this](float farDist) { camera.farPlane = glm::clamp(farDist, 0.0f, 10000.0f); };
    cam["getFarPlane"] = [this]() -> float { return camera.farPlane; };
    cam["setNearPlane"] = [this](float nearDist) { camera.nearPlane = glm::clamp(nearDist, 0.0f, 10000.0f); };
    cam["getNearPlane"] = [this]() -> float { return camera.nearPlane; };

    // ===== SUN LIGHT API =====
    sol::table sun = lua.create_named_table("Sun");
    sun["setDirection"] = [this](const LuaVec3& dir) { sunLight.direction = glm::normalize(dir.toGlm()); };
    sun["getDirection"] = [this]() -> LuaVec3 { return LuaVec3(sunLight.direction); };
    sun["setColor"] = [this](const LuaVec3& color) { sunLight.color = color.toGlm(); };
    sun["getColor"] = [this]() -> LuaVec3 { return LuaVec3(sunLight.color); };
    sun["setIntensity"] = [this](float intensity) { sunLight.intensity = intensity; };
    sun["getIntensity"] = [this]() -> float { return sunLight.intensity; };

    // Sun — обычная таблица, а не usertype, поэтому sol::property на ней не
    // работает и присваивание Sun.direction = ... просто создавало поле в
    // таблице, ничего не меняя (на это попались три теста в репозитории).
    // Метатаблица закрывает дыру: свойства работают как свойства, а опечатка
    // в имени теперь громкая ошибка, а не тишина.
    {
        sol::table sunMeta = lua.create_table();
        sunMeta["__index"] = [this](sol::this_state ts, sol::table, const std::string& key) -> sol::object {
            sol::state_view lua(ts);
            if (key == "direction") return sol::make_object(lua, LuaVec3(sunLight.direction));
            if (key == "color")     return sol::make_object(lua, LuaVec3(sunLight.color));
            if (key == "intensity") return sol::make_object(lua, sunLight.intensity);
            return sol::nil;
        };
        sunMeta["__newindex"] = [this](sol::table, const std::string& key, sol::object value) {
            if (key == "direction" && value.is<LuaVec3>()) {
                sunLight.direction = glm::normalize(value.as<LuaVec3>().toGlm());
            } else if (key == "color" && value.is<LuaVec3>()) {
                sunLight.color = value.as<LuaVec3>().toGlm();
            } else if (key == "intensity" && value.is<float>()) {
                sunLight.intensity = value.as<float>();
            } else {
                m_scriptEngine->log("Sun: нельзя присвоить '" + key +
                                    "' (доступны direction, color, intensity)",
                                    ConsoleLogEntry::Error);
            }
        };
        sun[sol::metatable_key] = sunMeta;
    }

    // ===== OTHER APIs =====
    bindPhysicsAPI();
    bindRmlUIAPI();

    // ===== Material API =====
    lua.set_function("setMaterialParam", &SceneEditorApp::setMaterialParam, this);
    lua.set_function("getMaterialParam", &SceneEditorApp::getMaterialParam, this);

	// ===== MATERIALS API =====
	sol::table materials = lua.create_named_table("Material");

	// Хендл материала по имени; 0 (INVALID_HANDLE), если такого нет.
	// Сравнивать материалы числами в скрипте заметно дешевле, чем строками:
	// obj.material каждый раз строит имя по хендлу обратным поиском.
	materials["getHandleByName"] = [this](const std::string& name) -> TextureHandle {
		return graphics->getMaterialHandleByName(name);
	};

	// Обратное преобразование — пригодится для вывода и отладки
	materials["getNameByHandle"] = [this](MaterialHandle handle) -> std::string {
		return getMaterialNameByHandle(handle);
	};

	materials["create"] = [this](const std::string& name,
								 TextureHandle diffH,
								 TextureHandle nmapH,
								 TextureHandle mraoH,
								 TextureHandle emapH,
								 float metallic,
								 float roughness,
								 float ao,
								 float uvScale,
								 const LuaVec2& uvOffset,
								 const LuaVec4& tintColor,
								 uint32_t flags,
								 const int filteringType,
								 sol::optional<int> shaderIndex,
								 sol::optional<bool> isDistortion) -> bool
	{
		// Проверяем, что имя не занято
		for (const auto& mat : availableMaterials) {
			if (mat.name == name) {
				m_scriptEngine->log("Material name already exists: " + name, ConsoleLogEntry::Warning);
				return false;
			}
		}

		Material mat;
		mat.name = name;
		mat.albedoMap = diffH;
		mat.normalMap = nmapH;
		mat.mraoMap = mraoH;
		mat.emissiveMap = emapH;
		mat.metallic = metallic;
		mat.roughness = roughness;
		mat.ao = ao;
		mat.uvScale = uvScale;
		mat.uvOffset = uvOffset.toGlm();
		mat.tintColor = tintColor.toGlm();
		mat.flags = flags;
		mat.filteringType = static_cast<MaterialFiltering>(filteringType);

		// Номер шейдерной программы: по умолчанию 0 (обычный материал)
		const int requestedShader = shaderIndex.value_or(0);
		if (requestedShader < 0 || requestedShader > 255) {
			m_scriptEngine->log("Material.create: номер шейдера вне 0..255", ConsoleLogEntry::Error);
			return false;
		}
		mat.shaderIndex = static_cast<uint8_t>(requestedShader);
		mat.isDistortion = isDistortion.value_or(false);

		MaterialHandle handle = graphics->createMaterial(mat);
		if (handle == INVALID_HANDLE) {
			m_scriptEngine->log("Failed to create material: " + name, ConsoleLogEntry::Error);
			return false;
		}

		MaterialAsset asset;
		asset.name = name;
		asset.handle = handle;
		asset.albedoPath = "";  // не используется для динамических материалов
		asset.normalPath = "";
		asset.mraoPath = "";
		asset.emapPath = "";
		asset.hasMRAO = (mraoH != INVALID_HANDLE);
		asset.hasEmap = (emapH != INVALID_HANDLE);
		asset.worlduv = (flags & PBR_FLAG_WORLD_UV) != 0;
		asset.uvscale = uvScale;
		asset.filteringType = mat.filteringType;
		asset.shaderIndex = mat.shaderIndex;
		asset.isDistortion = mat.isDistortion;
		availableMaterials.push_back(asset);

		m_scriptEngine->log("Material created: " + name, ConsoleLogEntry::Info);
		return true;
	};

	materials["delete"] = [this](const std::string& name) {
		auto it = std::find_if(availableMaterials.begin(), availableMaterials.end(),
			[&name](const MaterialAsset& m) { return m.name == name; });
		if (it != availableMaterials.end()) {
			availableMaterials.erase(it);
			m_scriptEngine->log("Material removed from list: " + name, ConsoleLogEntry::Info);
		} else {
			m_scriptEngine->log("Material not found: " + name, ConsoleLogEntry::Warning);
		}
	};

	// ===== Graphics: получить хендл рендер-текстуры слота =====



    // ===== Scene API (перегружено: строка | ObjectRef | ID) =====
    sol::table scene = lua["Scene"];
    scene["getTime"] = []() -> float { return static_cast<float>(glfwGetTime()); };

    scene["exists"] = [this](sol::object objArg) -> bool {
        return resolveObjectArg(objArg) != nullptr;
    };
    scene["getPosition"] = [this](sol::object objArg) -> LuaVec3 {
        GameObject* go = resolveObjectArg(objArg);
        return go ? LuaVec3(go->position) : LuaVec3(0);
    };
    scene["setPosition"] = [this](sol::object objArg, const LuaVec3& pos) {
        if (GameObject* go = resolveObjectArg(objArg)) {
            go->position = pos.toGlm();
            if (auto* body = getComponent<PhysicsBodyComponent>(*go))
                body->moved = true;
        }
    };
    scene["getRotation"] = [this](sol::object objArg) -> LuaVec3 {
        GameObject* go = resolveObjectArg(objArg);
        return go ? LuaVec3(go->rotation) : LuaVec3(0);
    };
    scene["setRotation"] = [this](sol::object objArg, const LuaVec3& rot) {
        if (GameObject* go = resolveObjectArg(objArg)) {
            go->rotation = rot.toGlm();
            if (auto* body = getComponent<PhysicsBodyComponent>(*go))
                body->moved = true;
        }
    };
    scene["getScale"] = [this](sol::object objArg) -> LuaVec3 {
        GameObject* go = resolveObjectArg(objArg);
        return go ? LuaVec3(go->scale) : LuaVec3(1);
    };
    scene["setScale"] = [this](sol::object objArg, const LuaVec3& scale) {
        if (GameObject* go = resolveObjectArg(objArg))
            go->scale = scale.toGlm();
    };
	scene["create"] = [this](const std::string& name, const std::string& mesh, const std::string& material) -> bool {
		uint32_t id = addGameObject("body", name);
		if (auto* body = getComponent<MeshComponent>(*getObjectById(id))) {
			body->renderable = getMeshHandleByName(mesh);
			body->materialHandle = getMaterialHandleByName(material);
			// === ОБНОВЛЕНИЕ localBounds ===
			if (const MeshAsset* m = getMeshByName(mesh)) {
				body->modelOffset = m->offset;
				body->localBounds = m->bounds;
			}
		}
		return true;
	};
    scene["destroy"] = [this](sol::object objArg) {
        if (GameObject* go = resolveObjectArg(objArg))
            removeGameObject(go->id);
    };
    scene["list"] = [this, &lua]() -> sol::table {
        sol::table result = lua.create_table();
        int idx = 1;
        for (const auto& go : gameObjects) result[idx++] = go.name;
        return result;
    };

    // ===== Light API (перегружено: строка | ObjectRef | ID) =====
    sol::table light = lua["Light"];

    light["setPosition"] = [this](sol::object objArg, const LuaVec3& pos) {
        if (GameObject* go = resolveObjectArg(objArg)) go->position = pos.toGlm();
    };
    light["getPosition"] = [this](sol::object objArg) -> LuaVec3 {
        if (GameObject* go = resolveObjectArg(objArg)) return LuaVec3(go->position);
        return LuaVec3(0);
    };
    light["setColor"] = [this](sol::object objArg, const LuaVec3& color) {
        if (GameObject* go = resolveObjectArg(objArg))
            if (auto* l = getComponent<LightComponent>(*go)) l->color = color.toGlm();
    };
    light["getColor"] = [this](sol::object objArg) -> LuaVec3 {
        if (GameObject* go = resolveObjectArg(objArg))
            if (auto* l = getComponent<LightComponent>(*go)) return LuaVec3(l->color);
        return LuaVec3(1);
    };
    light["setIntensity"] = [this](sol::object objArg, float intensity) {
        if (GameObject* go = resolveObjectArg(objArg))
            if (auto* l = getComponent<LightComponent>(*go)) l->intensity = intensity;
    };
    light["getIntensity"] = [this](sol::object objArg) -> float {
        if (GameObject* go = resolveObjectArg(objArg))
            if (auto* l = getComponent<LightComponent>(*go)) return l->intensity;
        return 0.0f;
    };
    light["setRadius"] = [this](sol::object objArg, float radius) {
        if (GameObject* go = resolveObjectArg(objArg))
            if (auto* l = getComponent<LightComponent>(*go)) l->radius = radius;
    };
    light["setEnabled"] = [this](sol::object objArg, bool enabled) {
        if (GameObject* go = resolveObjectArg(objArg)) go->enabled = enabled;
    };
    light["create"] = [this](const std::string& name) -> bool {
        addGameObject("light", name);
        return true;
    };
    light["destroy"] = [this](sol::object objArg) {
        if (GameObject* go = resolveObjectArg(objArg))
            removeGameObject(go->id);
    };
    light["list"] = [this, &lua]() -> sol::table {
        sol::table result = lua.create_table();
        int idx = 1;
        for (const auto& go : gameObjects)
            if (getComponent<LightComponent>(go)) result[idx++] = go.name;
        return result;
    };

    // ===== Resources API =====
    sol::table res = lua.create_named_table("Resources");
    res["load"] = [this](const std::string& path) {
        if (!isPathSafe(path)) {
            m_scriptEngine->log("Resources.load: Access denied to path: " + path, ConsoleLogEntry::Error);
            return;
        }
        loadResourcesFromJson(path);
    };
    res["unloadAll"] = [this]() { unloadAllResources(); };
	res["unloadSingle"] = [this](const std::string& type, const std::string& name) {
        unloadSingleResource(type, name);
    };
    res["loadSingle"] = [this](const std::string& type, const std::string& name, const std::string& path) {
        loadSingleResource(type, name, path);
    };
    res["getLoaded"] = [this]() -> sol::table {
        sol::state& lua = m_scriptEngine->lua();
        sol::table result = lua.create_table();

        sol::table meshes = lua.create_table();
        for (size_t i = 0; i < availableMeshes.size(); ++i) meshes[i + 1] = availableMeshes[i].name;
        result["mesh"] = meshes;

        sol::table materials = lua.create_table();
        for (size_t i = 0; i < availableMaterials.size(); ++i) materials[i + 1] = availableMaterials[i].name;
        result["material"] = materials;

        sol::table textures = lua.create_table();
        for (size_t i = 0; i < availableTextures.size(); ++i) textures[i + 1] = availableTextures[i].name;
        result["texture"] = textures;

        sol::table sounds = lua.create_table();
        for (size_t i = 0; i < m_loadedSoundNames.size(); ++i) sounds[i + 1] = m_loadedSoundNames[i];
        result["sound"] = sounds;

        sol::table fonts = lua.create_table();
        for (size_t i = 0; i < m_loadedFontPaths.size(); ++i) fonts[i + 1] = m_loadedFontPaths[i];
        result["font"] = fonts;

        return result;
    };

    // ============================================================
    // VIDEO API — управление видеотекстурами
    // ============================================================
    // Видеотекстура создаётся обычным Resources.loadSingle("texture", ...)
    // с путём к .mp4/.gif — тип определяется по расширению.
    sol::table video = lua.create_named_table("Video");

    // Все функции принимают имя ресурса ИЛИ хендл (число от Video.getHandle):
    // поиск по имени перебирает все текстуры со сравнением строк, и вызов
    // вроде Video.getTime(name) из update стоил этого перебора каждый кадр.
    auto resolveVideo = [this](const sol::object& arg, const char* who) -> TextureHandle {
        TextureHandle handle = INVALID_HANDLE;
        if (arg.is<std::string>()) {
            handle = graphics->getTextureHandleByName(arg.as<std::string>());
        } else if (arg.is<TextureHandle>()) {
            handle = arg.as<TextureHandle>();
        }
        if (!GraphicsEngineGL::isVideoHandle(handle) || !graphics->isVideoLoaded(handle)) {
            // Молчать нельзя: опечатка в имени раньше просто ничего не делала
            if (who) {
                std::string what = arg.is<std::string>() ? arg.as<std::string>() : std::string("<хендл>");
                m_scriptEngine->log(std::string("Video.") + who + ": видеотекстуры '" + what + "' нет",
                                    ConsoleLogEntry::Warning);
            }
            return INVALID_HANDLE;
        }
        return handle;
    };

    // Хендл для горячих мест: получить один раз, дальше звать Video.* с ним
    video["getHandle"] = [resolveVideo](const sol::object& arg) -> TextureHandle {
        return resolveVideo(arg, nullptr);
    };
    video["isVideo"] = [resolveVideo](const sol::object& arg) -> bool {
        return resolveVideo(arg, nullptr) != INVALID_HANDLE;
    };
    video["play"] = [this, resolveVideo](const sol::object& arg) {
        graphics->setVideoPlaying(resolveVideo(arg, "play"), true);
    };
    video["pause"] = [this, resolveVideo](const sol::object& arg) {
        graphics->setVideoPlaying(resolveVideo(arg, "pause"), false);
    };
    video["setLoop"] = [this, resolveVideo](const sol::object& arg, bool loop) {
        graphics->setVideoLoop(resolveVideo(arg, "setLoop"), loop);
    };
    video["setSpeed"] = [this, resolveVideo](const sol::object& arg, float speed) {
        graphics->setVideoSpeed(resolveVideo(arg, "setSpeed"), speed);
    };
    video["seek"] = [this, resolveVideo](const sol::object& arg, float seconds) {
        graphics->setVideoTime(resolveVideo(arg, "seek"), seconds);
    };
    video["getTime"] = [this, resolveVideo](const sol::object& arg) -> float {
        return graphics->getVideoTime(resolveVideo(arg, "getTime"));
    };
    video["getDuration"] = [this, resolveVideo](const sol::object& arg) -> float {
        return graphics->getVideoDuration(resolveVideo(arg, "getDuration"));
    };
    video["isPlaying"] = [this, resolveVideo](const sol::object& arg) -> bool {
        return graphics->isVideoPlaying(resolveVideo(arg, "isPlaying"));
    };
}


void SceneEditorApp::bindRmlUIAPI() {
    sol::state& lua = m_scriptEngine->lua();
#ifdef USE_RMLUI
    if (!m_rml) return;
    sol::table rml = lua.create_named_table("RmlUi");

    // 1. Загрузка документа
    rml["loadDocument"] = [this, &lua](const std::string& path) -> sol::object {
        Rml::ElementDocument* doc = m_rml->loadDocument(path);
        if (!doc) return sol::nil;
        return sol::make_object(lua, doc);
    };

    // 2. Показать документ
    rml["show"] = [](sol::object docObj, int modalFlag, int focusFlag) {
        if (docObj.is<Rml::ElementDocument*>()) {
            Rml::ElementDocument* doc = docObj.as<Rml::ElementDocument*>();
            if (doc) doc->Show(static_cast<Rml::ModalFlag>(modalFlag), static_cast<Rml::FocusFlag>(focusFlag));
        }
    };

    // 3. Скрыть документ
    rml["hide"] = [](sol::object docObj) {
        if (docObj.is<Rml::ElementDocument*>()) {
            Rml::ElementDocument* doc = docObj.as<Rml::ElementDocument*>();
            if (doc) doc->Hide();
        }
    };

    // 4. Получить элемент по ID из документа
    rml["getElementById"] = [&lua](sol::object docObj, const std::string& id) -> sol::object {
        if (!docObj.is<Rml::ElementDocument*>()) return sol::nil;
        Rml::ElementDocument* doc = docObj.as<Rml::ElementDocument*>();
        if (!doc) return sol::nil;
        Rml::Element* el = doc->GetElementById(id);
        if (!el) return sol::nil;
        return sol::make_object(lua, el);
    };

    // 5. Установить внутренний RML (innerHTML)
    rml["setInnerRML"] = [](sol::object elemObj, const std::string& rmlText) {
        if (elemObj.is<Rml::Element*>()) {
            Rml::Element* el = elemObj.as<Rml::Element*>();
            if (el) el->SetInnerRML(rmlText);
        }
    };

    // 6. Установить атрибут элемента
    rml["setAttribute"] = [](sol::object elemObj, const std::string& attr, sol::object value) {
        if (!elemObj.is<Rml::Element*>()) return;
        Rml::Element* el = elemObj.as<Rml::Element*>();
        if (!el) return;
        if (value.is<std::string>())
            el->SetAttribute(attr, value.as<std::string>());
        else if (value.is<float>())
            el->SetAttribute(attr, value.as<float>());
        else if (value.is<int>())
            el->SetAttribute(attr, value.as<int>());
        else if (value.is<bool>())
            el->SetAttribute(attr, value.as<bool>() ? "true" : "false");
    };

    // 7. Получить атрибут (возвращает строку)
    rml["getAttribute"] = [](sol::object elemObj, const std::string& attr) -> std::string {
        if (!elemObj.is<Rml::Element*>()) return "";
        Rml::Element* el = elemObj.as<Rml::Element*>();
        if (!el) return "";
        Rml::Variant* var = el->GetAttribute(attr);
        if (!var) return "";
        return var->Get<Rml::String>();
    };

    // 8. Добавление обработчика события
    //static std::unordered_map<Rml::Element*, std::vector<sol::protected_function>> eventListeners;
    rml["addEventListener"] = [this](sol::object elemObj, const std::string& event, sol::function callback) {
        if (!elemObj.is<Rml::Element*>()) {
            m_scriptEngine->log("RmlUi: addEventListener on invalid element", ConsoleLogEntry::Error);
            return;
        }
        Rml::Element* el = elemObj.as<Rml::Element*>();
        if (!el) return;

        m_eventListeners[el].push_back(callback);

        class LuaEventListener : public Rml::EventListener {
        public:
            LuaEventListener(sol::protected_function cb, ScriptEngine* engine)
                : m_callback(cb), m_engine(engine) {}
            void ProcessEvent(Rml::Event& ev) override {
                auto result = m_callback(ev.GetType().c_str());
                if (!result.valid()) {
                    sol::error err = result;
                    m_engine->log("[RmlUi event] " + std::string(err.what()), ConsoleLogEntry::Error);
                }
            }
        private:
            sol::protected_function m_callback;
            ScriptEngine* m_engine;
        };

        el->AddEventListener(event, new LuaEventListener(callback, m_scriptEngine.get()), true);
    };

    // 9. Полное удаление документа из памяти
    rml["destroy"] = [this](sol::object docObj) {
        if (!m_rml || !docObj.is<Rml::ElementDocument*>()) return;
        Rml::ElementDocument* doc = docObj.as<Rml::ElementDocument*>();
        if (doc) {
            // UnloadDocument удаляет документ и всю его геометрию из памяти
            m_rml->getContext()->UnloadDocument(doc);
        }
    };

    // 10. Установить CSS-свойство (для динамического изменения стиля)
    rml["setProperty"] = [](sol::object elemObj, const std::string& prop, const std::string& value) {
        if (!elemObj.is<Rml::Element*>()) return;
        Rml::Element* el = elemObj.as<Rml::Element*>();
        if (!el) return;
        el->SetProperty(prop, value);
    };
#else
    // Сборка без RmlUi: регистрируем таблицу-заглушку, чтобы скрипты,
    // обращающиеся к RmlUi, падали предсказуемо, а не на nil-индексации.
    {
        sol::table rml = lua.create_named_table("RmlUi");
        rml["available"] = false;
        auto notAvailable = [this]() {
            m_scriptEngine->log("RmlUi: движок собран с USE_RMLUI=OFF", ConsoleLogEntry::Warning);
        };
        rml["loadDocument"]     = [notAvailable](const std::string&) -> sol::object { notAvailable(); return sol::nil; };
        rml["show"]             = [notAvailable](sol::object, int, int) { notAvailable(); };
        rml["hide"]             = [notAvailable](sol::object) { notAvailable(); };
        rml["destroy"]          = [notAvailable](sol::object) { notAvailable(); };
        rml["getElementById"]   = [notAvailable](sol::object, const std::string&) -> sol::object { notAvailable(); return sol::nil; };
        rml["setInnerRML"]      = [notAvailable](sol::object, const std::string&) { notAvailable(); };
        rml["setAttribute"]     = [notAvailable](sol::object, const std::string&, sol::object) { notAvailable(); };
        rml["getAttribute"]     = [notAvailable](sol::object, const std::string&) -> std::string { notAvailable(); return ""; };
        rml["setProperty"]      = [notAvailable](sol::object, const std::string&, const std::string&) { notAvailable(); };
        rml["addEventListener"] = [notAvailable](sol::object, const std::string&, sol::function) { notAvailable(); };
    }
#endif // USE_RMLUI

	// ============================================================
	// GRAPHICS API
	// ============================================================
	// Легкая обёртка для Lua, чтобы sol2 мог корректно использовать sol::property
	struct LuaGraphicsAPI {
		GraphicsEngineGL* engine;
		QualitySettings* settings;
		ScriptEngine* scriptEngine; // Нужен для логов (типа MSAA warning)

		LuaGraphicsAPI(GraphicsEngineGL* eng, QualitySettings* set, ScriptEngine* se)
			: engine(eng), settings(set), scriptEngine(se) {}

		// Обычный метод (не property)
		TextureHandle getRenderTargetHandle(int slot) {
			return engine->getRenderTargetHandle(slot);
		}

		void resizeGraphics() {
			engine->resize(width, height);
		}
	};

	// ============================================================================
	// 1. РЕГИСТРАЦИЯ ТИПА (ДЕЛАЕТСЯ 1 РАЗ ПРИ СТАРТЕ LUA)
	// ============================================================================
	auto graphicsType = lua.new_usertype<LuaGraphicsAPI>("Graphics", sol::no_constructor);

	// Свойства регистрируются по одному: sol2 разворачивает переменный список
	// аргументов new_usertype в одну гигантскую инстанциацию шаблона, и на сотне
	// свойств компилятору перестаёт хватать памяти.

	// --- Обычные методы ---
	graphicsType["getRenderTargetHandle"] = &LuaGraphicsAPI::getRenderTargetHandle;
	graphicsType["resize"] = &LuaGraphicsAPI::resizeGraphics;

	// --- Custom Params ---
	graphicsType["customParam1"] = sol::property(
	[](const LuaGraphicsAPI& self) -> LuaVec4 { return LuaVec4(self.settings->customParam1); },
	[](LuaGraphicsAPI& self, const LuaVec4& val) { self.settings->customParam1 = val.toGlm(); self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["customParam2"] = sol::property(
	[](const LuaGraphicsAPI& self) -> LuaVec4 { return LuaVec4(self.settings->customParam2); },
	[](LuaGraphicsAPI& self, const LuaVec4& val) { self.settings->customParam2 = val.toGlm(); self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["customParam3"] = sol::property(
	[](const LuaGraphicsAPI& self) -> LuaVec4 { return LuaVec4(glm::vec4(self.settings->customParam3)); },
	[](LuaGraphicsAPI& self, const LuaVec4& val) { self.settings->customParam3 = glm::ivec4(val.toGlm()); self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["customParam4"] = sol::property(
	[](const LuaGraphicsAPI& self) -> LuaVec4 { return LuaVec4(glm::vec4(self.settings->customParam4)); },
	[](LuaGraphicsAPI& self, const LuaVec4& val) { self.settings->customParam4 = glm::ivec4(val.toGlm()); self.engine->applyGraphicsSettings(*self.settings); }
	);

	// --- Основные параметры ---
	graphicsType["exposure"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->exposure; },
	[](LuaGraphicsAPI& self, float val) { self.settings->exposure = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["gamma"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->gamma; },
	[](LuaGraphicsAPI& self, float val) { self.settings->gamma = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["useGammaCorrection"] = sol::property(
	[](const LuaGraphicsAPI& self) -> bool { return self.settings->useGammaCorrection; },
	[](LuaGraphicsAPI& self, bool val) { self.settings->useGammaCorrection = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["ambientIntensity"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->ambientIntensity; },
	[](LuaGraphicsAPI& self, float val) { self.settings->ambientIntensity = val; self.engine->applyGraphicsSettings(*self.settings); }
	);

	// --- Тени ---
	// Тени точечных и конусных источников: этих трёх настроек в Lua не было
	// вовсе, хотя рядом выведена вся мелочь вроде ssrThickness и godraysDecay
	graphicsType["usePointLightShadows"] = sol::property(
	[](const LuaGraphicsAPI& self) -> bool { return self.settings->usePointLightShadows; },
	[](LuaGraphicsAPI& self, bool val) { self.settings->usePointLightShadows = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["pointShadowMapResolution"] = sol::property(
	[](const LuaGraphicsAPI& self) -> int { return self.settings->pointShadowMapResolution; },
	[](LuaGraphicsAPI& self, int val) {
		// Пересоздание массива кубмап — не бесплатная операция, поэтому
		// ограничиваем разумными степенями двойки
		self.settings->pointShadowMapResolution = std::clamp(val, 64, 4096);
		self.engine->applyGraphicsSettings(*self.settings);
	}
	);
	graphicsType["maxShadowCastingPointLights"] = sol::property(
	[](const LuaGraphicsAPI& self) -> int { return self.settings->maxShadowCastingPointLights; },
	[](LuaGraphicsAPI& self, int val) {
		self.settings->maxShadowCastingPointLights = std::max(val, 0);
		self.engine->applyGraphicsSettings(*self.settings);
	}
	);

	graphicsType["shadowMapResolution"] = sol::property(
	[](const LuaGraphicsAPI& self) -> int { return self.settings->shadowMapResolution; },
	[](LuaGraphicsAPI& self, int val) { self.settings->shadowMapResolution = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["shadowBias"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->shadowBias; },
	[](LuaGraphicsAPI& self, float val) { self.settings->shadowBias = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["shadowNormalOffset"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->shadowNormalOffset; },
	[](LuaGraphicsAPI& self, float val) { self.settings->shadowNormalOffset = val; self.engine->applyGraphicsSettings(*self.settings); }
	);

	// --- Туман ---
	graphicsType["fogColor"] = sol::property(
	[](const LuaGraphicsAPI& self) -> LuaVec3 { return LuaVec3(self.settings->fogColor); },
	[](LuaGraphicsAPI& self, const LuaVec3& val) { self.settings->fogColor = val.toGlm(); self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["fogDensity"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->fogDensity; },
	[](LuaGraphicsAPI& self, float val) { self.settings->fogDensity = val; self.engine->applyGraphicsSettings(*self.settings); }
	);

	// --- Пост-процессинг ---
	graphicsType["usePostProcessing"] = sol::property(
	[](const LuaGraphicsAPI& self) -> bool { return self.settings->usePostProcessing; },
	[](LuaGraphicsAPI& self, bool val) { self.settings->usePostProcessing = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["bloomResolutionScale"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->bloomResolutionScale; },
	[](LuaGraphicsAPI& self, float val) { self.settings->bloomResolutionScale = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["ssaoResolutionScale"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->ssaoResolutionScale; },
	[](LuaGraphicsAPI& self, float val) { self.settings->ssaoResolutionScale = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["useBloom"] = sol::property(
	[](const LuaGraphicsAPI& self) -> bool { return self.settings->useBloom; },
	[](LuaGraphicsAPI& self, bool val) { self.settings->useBloom = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["bloomIntensity"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->bloomIntensity; },
	[](LuaGraphicsAPI& self, float val) { self.settings->bloomIntensity = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["bloomThreshold"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->bloomThreshold; },
	[](LuaGraphicsAPI& self, float val) { self.settings->bloomThreshold = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["bloomIterations"] = sol::property(
	[](const LuaGraphicsAPI& self) -> int { return self.settings->bloomIterations; },
	[](LuaGraphicsAPI& self, int val) { self.settings->bloomIterations = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["useSSAO"] = sol::property(
	[](const LuaGraphicsAPI& self) -> bool { return self.settings->useSSAO; },
	[](LuaGraphicsAPI& self, bool val) { self.settings->useSSAO = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["ssaoRadius"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->ssaoRadius; },
	[](LuaGraphicsAPI& self, float val) { self.settings->ssaoRadius = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	// Насколько туман съедает окклюзию: 1 — в плотном тумане SSAO не видно
	graphicsType["ssaoFogFade"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->ssaoFogFade; },
	[](LuaGraphicsAPI& self, float val) { self.settings->ssaoFogFade = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["ssaoBias"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->ssaoBias; },
	[](LuaGraphicsAPI& self, float val) { self.settings->ssaoBias = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["ssaoPower"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->ssaoPower; },
	[](LuaGraphicsAPI& self, float val) { self.settings->ssaoPower = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["useMotionBlur"] = sol::property(
	[](const LuaGraphicsAPI& self) -> bool { return self.settings->useMotionBlur; },
	[](LuaGraphicsAPI& self, bool val) { self.settings->useMotionBlur = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["motionBlurStrength"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->motionBlurStrength; },
	[](LuaGraphicsAPI& self, float val) { self.settings->motionBlurStrength = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["useChromaticAberration"] = sol::property(
	[](const LuaGraphicsAPI& self) -> bool { return self.settings->useChromaticAberration; },
	[](LuaGraphicsAPI& self, bool val) { self.settings->useChromaticAberration = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["chromaticAberrationStrength"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->chromaticAberrationStrength; },
	[](LuaGraphicsAPI& self, float val) { self.settings->chromaticAberrationStrength = val; self.engine->applyGraphicsSettings(*self.settings); }
	);

	// --- Туман ---
	graphicsType["fogMode"] = sol::property(
	[](const LuaGraphicsAPI& self) -> int { return self.settings->fogMode; },
	[](LuaGraphicsAPI& self, int val) { self.settings->fogMode = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["fogStart"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->fogStart; },
	[](LuaGraphicsAPI& self, float val) { self.settings->fogStart = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["fogEnd"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->fogEnd; },
	[](LuaGraphicsAPI& self, float val) { self.settings->fogEnd = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["fogHeightFalloff"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->fogHeightFalloff; },
	[](LuaGraphicsAPI& self, float val) { self.settings->fogHeightFalloff = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["fogBaseHeight"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->fogBaseHeight; },
	[](LuaGraphicsAPI& self, float val) { self.settings->fogBaseHeight = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["fogSunColor"] = sol::property(
	[](const LuaGraphicsAPI& self) -> LuaVec3 { return LuaVec3(self.settings->fogSunColor); },
	[](LuaGraphicsAPI& self, const LuaVec3& val) { self.settings->fogSunColor = val.toGlm(); self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["fogSunAmount"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->fogSunAmount; },
	[](LuaGraphicsAPI& self, float val) { self.settings->fogSunAmount = val; self.engine->applyGraphicsSettings(*self.settings); }
	);

	// --- Godrays ---
	graphicsType["useGodrays"] = sol::property(
	[](const LuaGraphicsAPI& self) -> bool { return self.settings->useGodrays; },
	[](LuaGraphicsAPI& self, bool val) { self.settings->useGodrays = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["godraysExposure"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->godraysExposure; },
	[](LuaGraphicsAPI& self, float val) { self.settings->godraysExposure = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["godraysDensity"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->godraysDensity; },
	[](LuaGraphicsAPI& self, float val) { self.settings->godraysDensity = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["godraysWeight"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->godraysWeight; },
	[](LuaGraphicsAPI& self, float val) { self.settings->godraysWeight = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["godraysDecay"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->godraysDecay; },
	[](LuaGraphicsAPI& self, float val) { self.settings->godraysDecay = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["godraysSamples"] = sol::property(
	[](const LuaGraphicsAPI& self) -> int { return self.settings->godraysSamples; },
	[](LuaGraphicsAPI& self, int val) {
		// Диапазон тот же, что в шейдере и в проверке cfg.json
		self.settings->godraysSamples = std::clamp(val, 8, 128);
		self.engine->applyGraphicsSettings(*self.settings);
	}
	);
	// Единственное поле лучей, которого раньше не было в Lua (соседние
	// bloomResolutionScale/ssaoResolutionScale выведены). Выводить его было
	// нельзя, пока смена масштаба не пересоздавала буфер — теперь пересоздаёт.
	graphicsType["godraysResolutionScale"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->godraysResolutionScale; },
	[](LuaGraphicsAPI& self, float val) {
		self.settings->godraysResolutionScale = std::clamp(val, 0.05f, 1.0f);
		self.engine->applyGraphicsSettings(*self.settings);
	}
	);

	// --- Цветокоррекция ---
	graphicsType["useColorGrading"] = sol::property(
	[](const LuaGraphicsAPI& self) -> bool { return self.settings->useColorGrading; },
	[](LuaGraphicsAPI& self, bool val) { self.settings->useColorGrading = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["contrast"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->contrast; },
	[](LuaGraphicsAPI& self, float val) { self.settings->contrast = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["temperature"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->temperature; },
	[](LuaGraphicsAPI& self, float val) { self.settings->temperature = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["tint"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->tint; },
	[](LuaGraphicsAPI& self, float val) { self.settings->tint = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["colorLift"] = sol::property(
	[](const LuaGraphicsAPI& self) -> LuaVec3 { return LuaVec3(self.settings->colorLift); },
	[](LuaGraphicsAPI& self, const LuaVec3& val) { self.settings->colorLift = val.toGlm(); self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["colorGamma"] = sol::property(
	[](const LuaGraphicsAPI& self) -> LuaVec3 { return LuaVec3(self.settings->colorGamma); },
	[](LuaGraphicsAPI& self, const LuaVec3& val) {
		// Гамма стоит в знаменателе показателя степени — ноль и минус недопустимы
		const glm::vec3 g = val.toGlm();
		self.settings->colorGamma = glm::max(g, glm::vec3(1e-3f));
		self.engine->applyGraphicsSettings(*self.settings);
	}
	);
	graphicsType["colorGain"] = sol::property(
	[](const LuaGraphicsAPI& self) -> LuaVec3 { return LuaVec3(self.settings->colorGain); },
	[](LuaGraphicsAPI& self, const LuaVec3& val) { self.settings->colorGain = val.toGlm(); self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["vignetteStrength"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->vignetteStrength; },
	[](LuaGraphicsAPI& self, float val) {
		self.settings->vignetteStrength = std::clamp(val, 0.0f, 1.0f);
		self.engine->applyGraphicsSettings(*self.settings);
	}
	);
	graphicsType["vignetteRadius"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->vignetteRadius; },
	[](LuaGraphicsAPI& self, float val) {
		// >= 1.0 даёт edge0 >= edge1 в smoothstep — по спецификации GLSL
		// это неопределённое поведение (на практике кадр становился чёрным)
		self.settings->vignetteRadius = std::clamp(val, 0.0f, 0.999f);
		self.engine->applyGraphicsSettings(*self.settings);
	}
	);

	// --- Radiance Cascades (непрямое освещение) ---
	graphicsType["useRadianceCascades"] = sol::property(
	[](const LuaGraphicsAPI& self) -> bool { return self.settings->useRadianceCascades; },
	[](LuaGraphicsAPI& self, bool val) { self.settings->useRadianceCascades = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["radianceCascadeCount"] = sol::property(
	[](const LuaGraphicsAPI& self) -> int { return self.settings->radianceCascadeCount; },
	[](LuaGraphicsAPI& self, int val) {
		// Диапазоны те же, что при чтении cfg.json: за пределами шейдер
		// либо делит на ноль, либо пишет за границы массива каскадов
		self.settings->radianceCascadeCount = std::clamp(val, 1, 6);
		self.engine->applyGraphicsSettings(*self.settings);
	}
	);
	graphicsType["radianceProbeSpacing"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->radianceProbeSpacing; },
	[](LuaGraphicsAPI& self, float val) {
		self.settings->radianceProbeSpacing = std::clamp(val, 2.0f, 64.0f);
		self.engine->applyGraphicsSettings(*self.settings);
	}
	);
	graphicsType["radianceBaseInterval"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->radianceBaseInterval; },
	[](LuaGraphicsAPI& self, float val) {
		self.settings->radianceBaseInterval = std::max(val, 0.01f);
		self.engine->applyGraphicsSettings(*self.settings);
	}
	);
	graphicsType["radianceRaySteps"] = sol::property(
	[](const LuaGraphicsAPI& self) -> int { return self.settings->radianceRaySteps; },
	[](LuaGraphicsAPI& self, int val) {
		self.settings->radianceRaySteps = std::clamp(val, 2, 64);
		self.engine->applyGraphicsSettings(*self.settings);
	}
	);
	graphicsType["radianceIntensity"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->radianceIntensity; },
	[](LuaGraphicsAPI& self, float val) {
		// Отрицательная интенсивность дала бы отрицательный свет, а слишком
		// большая — раскачку: карта GI подмешивается в сцену, которую читает
		// следующий кадр каскадов
		self.settings->radianceIntensity = std::clamp(val, 0.0f, 4.0f);
		self.engine->applyGraphicsSettings(*self.settings);
	}
	);
	graphicsType["useSSR"] = sol::property(
	[](const LuaGraphicsAPI& self) -> bool { return self.settings->useSSR; },
	[](LuaGraphicsAPI& self, bool val) { self.settings->useSSR = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["ssrMaxSteps"] = sol::property(
	[](const LuaGraphicsAPI& self) -> int { return self.settings->ssrMaxSteps; },
	[](LuaGraphicsAPI& self, int val) { self.settings->ssrMaxSteps = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["ssrThickness"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->ssrThickness; },
	[](LuaGraphicsAPI& self, float val) { self.settings->ssrThickness = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["ssrFadeDistance"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->ssrFadeDistance; },
	[](LuaGraphicsAPI& self, float val) { self.settings->ssrFadeDistance = val; self.engine->applyGraphicsSettings(*self.settings); }
	);

	// --- Режим рендеринга ---
	graphicsType["renderMode"] = sol::property(
	[](const LuaGraphicsAPI& self) -> int { return static_cast<int>(self.settings->renderMode); },
	[](LuaGraphicsAPI& self, int val) { self.settings->renderMode = static_cast<RenderMode>(val); self.engine->applyGraphicsSettings(*self.settings); }
	);

	// --- Текстурная фильтрация ---
	graphicsType["useAnisotropic"] = sol::property(
	[](const LuaGraphicsAPI& self) -> bool { return self.settings->useAnisotropic; },
	[](LuaGraphicsAPI& self, bool val) { self.settings->useAnisotropic = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["anisotropicLevel"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->anisotropicLevel; },
	[](LuaGraphicsAPI& self, float val) { self.settings->anisotropicLevel = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["useTrilinearFiltering"] = sol::property(
	[](const LuaGraphicsAPI& self) -> bool { return self.settings->useTrilinearFiltering; },
	[](LuaGraphicsAPI& self, bool val) { self.settings->useTrilinearFiltering = val; self.engine->applyGraphicsSettings(*self.settings); }
	);

	// --- Anti-Aliasing ---
	graphicsType["useMSAA"] = sol::property(
	[](const LuaGraphicsAPI& self) -> bool { return self.settings->useMSAA; },
	[](LuaGraphicsAPI& self, bool val) { 
	self.settings->useMSAA = val; 
	self.engine->applyGraphicsSettings(*self.settings); 
	if(self.scriptEngine) self.scriptEngine->log("MSAA change requires window restart to take full effect.", ConsoleLogEntry::Warning);
	}
	);
	graphicsType["msaaSamples"] = sol::property(
	[](const LuaGraphicsAPI& self) -> int { return self.settings->msaaSamples; },
	[](LuaGraphicsAPI& self, int val) { 
	self.settings->msaaSamples = val; 
	self.engine->applyGraphicsSettings(*self.settings); 
	if(self.scriptEngine) self.scriptEngine->log("MSAA change requires window restart to take full effect.", ConsoleLogEntry::Warning);
	}
	);

	// --- Forward+ и Light Culling ---
	graphicsType["useForwardPlus"] = sol::property(
	[](const LuaGraphicsAPI& self) -> bool { return self.settings->useForwardPlus; },
	[](LuaGraphicsAPI& self, bool val) { self.settings->useForwardPlus = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["useLightCulling"] = sol::property(
	[](const LuaGraphicsAPI& self) -> bool { return self.settings->useLightCulling; },
	[](LuaGraphicsAPI& self, bool val) { self.settings->useLightCulling = val; self.engine->applyGraphicsSettings(*self.settings); }
	);

	// --- Цветокоррекция и скайбокс ---
	graphicsType["brightness"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->brightness; },
	[](LuaGraphicsAPI& self, float val) { self.settings->brightness = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["blurAmount"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->blurAmount; },
	[](LuaGraphicsAPI& self, float val) { self.settings->blurAmount = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["saturation"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->saturation; },
	[](LuaGraphicsAPI& self, float val) { self.settings->saturation = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["skyboxBrightness"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->skyboxBrightness; },
	[](LuaGraphicsAPI& self, float val) { self.settings->skyboxBrightness = val; self.engine->applyGraphicsSettings(*self.settings); }
	);
	graphicsType["skyboxSaturation"] = sol::property(
	[](const LuaGraphicsAPI& self) -> float { return self.settings->skyboxSaturation; },
	[](LuaGraphicsAPI& self, float val) { self.settings->skyboxSaturation = val; self.engine->applyGraphicsSettings(*self.settings); }
	);


	// Регистрация шейдерной программы из скрипта. После startGraphics()
	// программа компилируется сразу же, так что можно и переопределять на лету.
	graphicsType["addShaderProgram"] = [this](LuaGraphicsAPI& self, int num,
	                                          const std::string& vert, const std::string& frag,
	                                          sol::optional<std::string> geom) -> bool {
		if (num < 0 || num > 255) {
			m_scriptEngine->log("Graphics.addShaderProgram: номер вне 0..255", ConsoleLogEntry::Error);
			return false;
		}
		if (!isPathSafe(vert) || !isPathSafe(frag) ||
		    (geom && !isPathSafe(*geom))) {
			m_scriptEngine->log("Graphics.addShaderProgram: Access denied to path", ConsoleLogEntry::Error);
			return false;
		}
		self.engine->addShaderProgram(static_cast<uint8_t>(num), vert, frag,
		                              geom.value_or(std::string()));
		if (!self.engine->isShaderProgramValid(static_cast<uint8_t>(num))) {
			m_scriptEngine->log("Graphics.addShaderProgram: программа " + std::to_string(num) +
			                    " не собралась, см. лог компиляции", ConsoleLogEntry::Error);
			return false;
		}
		return true;
	};

	graphicsType["shaderProgramCount"] = [](const LuaGraphicsAPI& self) -> int {
		return static_cast<int>(self.engine->shaderProgramCount());
	};

	// Счётчики последнего кадра: по ним видно, работают ли отсечение
	// и инстансинг, в том числе при прогоне без окна
	graphicsType["frameStats"] = [this](const LuaGraphicsAPI& self) -> sol::table {
		sol::table t = m_scriptEngine->lua().create_table();
		const auto& f = self.engine->getFrameStats();
		const auto& c = self.engine->getCullingStats();
		t["drawCalls"]        = f.drawCalls;
		t["shadowDrawCalls"]  = f.shadowDrawCalls;
		t["instancedBatches"] = f.instancedBatches;
		t["itemsSubmitted"]   = f.itemsSubmitted;
		t["totalObjects"]     = c.totalObjects;
		t["frustumCulled"]    = c.frustumCulled;
		t["rendered"]         = c.rendered;
		return t;
	};

	// Отладочная отрисовка костей: те же флаги, что и чекбоксы в Graphics
	// Settings. Из скрипта они нужны визуальным тестам — без окна галку не ткнёшь.
	graphicsType["showSkeletonDebug"] = sol::property(
		[this](const LuaGraphicsAPI&) { return m_showSkeletonDebug; },
		[this](const LuaGraphicsAPI&, bool v) { m_showSkeletonDebug = v; });
	graphicsType["skeletonDebugBoneSize"] = sol::property(
		[this](const LuaGraphicsAPI&) { return m_skeletonDebugBoneSize; },
		[this](const LuaGraphicsAPI&, float v) { m_skeletonDebugBoneSize = std::max(v, 0.001f); });
	graphicsType["skeletonDebugOnTop"] = sol::property(
		[this](const LuaGraphicsAPI&) { return m_skeletonDebugOnTop; },
		[this](const LuaGraphicsAPI&, bool v) { m_skeletonDebugOnTop = v; });
	graphicsType["showPhysicsDebug"] = sol::property(
		[this](const LuaGraphicsAPI&) { return m_showPhysicsDebug; },
		[this](const LuaGraphicsAPI&, bool v) { m_showPhysicsDebug = v; });

	// ============================================================
	// 2. СОЗДАНИЕ ЭКЗЕМПЛЯРА И ПРИВЯЗКА К ГЛОБАЛЬНОМУ "Graphics"
	// ============================================================
	// Создаем объект в C++ и отдаем его в Lua. 
	// Теперь Graphics в Lua - это userdata, а не table!
	lua["Graphics"] = LuaGraphicsAPI(graphics.get(), &m_qualitySettings, m_scriptEngine.get());
}


void SceneEditorApp::bindPhysicsAPI() {
    sol::state& lua = m_scriptEngine->lua();
    sol::table physics = lua["Physics"];

    // ===== COLLISION GROUP CONSTANTS =====
    // Подтаблицы физики живут под Physics.*: так видно, к какой подсистеме
    // относится вызов. Старые глобальные имена оставлены псевдонимами на те же
    // таблицы — существующие скрипты продолжают работать, но в новом коде
    // пользуйтесь строгой формой Physics.ColGroup / Physics.Ragdoll / …
    sol::table colGroup = lua.create_table();
    physics["ColGroup"] = colGroup;
    lua["ColGroup"] = colGroup;
    colGroup["NOTHING"]  = PhysicsEngine::COL_NOTHING;
    colGroup["WORLD"]    = PhysicsEngine::COL_WORLD;
    colGroup["MOVEMENT"] = PhysicsEngine::COL_MOVEMENT;
    colGroup["HITBOX"]   = PhysicsEngine::COL_HITBOX;
    colGroup["DYNAMIC"]  = PhysicsEngine::COL_DYNAMIC;
    colGroup["TRIGGER"]  = PhysicsEngine::COL_TRIGGER;
    colGroup["ALL"]      = PhysicsEngine::COL_ALL;

    physics["createBody"] = [this](sol::object objArg, float mass,
        const LuaVec3& dimensions,
        sol::optional<std::string> shapeType,
        sol::optional<uint32_t> group,
        sol::optional<uint32_t> mask) -> bool
    {
        GameObject* go = resolveObjectArg(objArg);
        if (!go) return false;

        // Работаем с ОБЁРТКОЙ компонента, а не только с его данными: телу
        // нужна мировая точка компонента. Раньше тело создавалось в
        // go->position, а loadScene поднимал его в comp.worldPosition —
        // сцена вела себя по-разному до и после перезагрузки.
        go->updateComponentTransforms();
        Component* bodyComp = nullptr;
        for (auto& c : go->components)
            if (std::holds_alternative<PhysicsBodyComponent>(c.data)) { bodyComp = &c; break; }
        if (!bodyComp) {
            bodyComp = &go->addComponent(PhysicsBodyComponent{});
            bodyComp->worldPosition = go->position;
            bodyComp->worldRotationQuat = go->rotationQuat;
        }
        PhysicsBodyComponent* body = std::get_if<PhysicsBodyComponent>(&bodyComp->data);
        // Форму по мешу берём у меша того же объекта
        const MeshComponent* meshComp = getComponent<MeshComponent>(*go);

        // shapeId совпадает с PhysicsEngine::CollisionShape по значениям
        int shapeId = 0;
        if (shapeType.has_value()) {
            const std::string& type = shapeType.value();
            if      (type == "sphere")   shapeId = 1;
            else if (type == "capsule")  shapeId = 2;
            else if (type == "cylinder") shapeId = 3;
            else if (type == "convex")   shapeId = 4;
            else if (type == "trimesh")  shapeId = 5;
            else if (type == "cone")     shapeId = 6;
            else if (type == "plane")    shapeId = 7;
            else if (type != "box") {
                m_scriptEngine->log("Physics.createBody: неизвестная форма '" + type + "', берётся box",
                                    ConsoleLogEntry::Warning);
            }
        }

        const glm::quat initialRot = bodyComp->worldRotationQuat;
        const uint32_t colGroup = group.value_or(PhysicsEngine::COL_WORLD);
        const uint32_t colMask  = mask.value_or(PhysicsEngine::COL_ALL);

        // Мешевому коллайдеру нужен меш — проверяем ДО правки компонента,
        // иначе при отказе объект остаётся с чужим shapeType и размерами
        const std::string meshHint = meshComp ? getMeshNameByHandle(meshComp->renderable)
                                              : std::string();
        if ((shapeId == 4 || shapeId == 5) && meshHint.empty()) {
            m_scriptEngine->log("Physics.createBody: на объекте нет меша для мешевого коллайдера",
                                ConsoleLogEntry::Error);
            return false;
        }

        // Заполняем компонент ДО создания тела: createBodyForComponent читает
        // именно из него (и он же единственный источник правды для сохранения)
        body->shapeType = shapeId;
        // Форму назвали явно — файл коллайдера здесь не должен её перебивать
        body->colliderFile.clear();
        body->mass = mass;
        body->collisionDimensions = dimensions.toGlm();
        body->collisionGroup = colGroup;
        body->collisionMask = colMask;

        const PhysicsHandle handle =
            createBodyForComponent(*body, bodyComp->worldPosition, initialRot, meshHint);
        if (handle != INVALID_HANDLE) {
            body->physicsBody = handle;
            body->physicsEnabled = true;
            body->collisionEnabled = true;
            return true;
        }
        return false;
    };
	
	// --- Триггеры ---
	physics["setTrigger"] = [this](sol::object objArg, bool isTrigger) {
		if (PhysicsBodyComponent* body = resolveBodyArg(objArg))
			m_physics->setTrigger(body->physicsBody, isTrigger);
	};

	// --- Продвинутые Рейкасты ---
	physics["raycastAll"] = [this, &lua](const LuaVec3& from, const LuaVec3& to, sol::optional<uint32_t> maskOpt) -> sol::table {
		uint32_t mask = maskOpt.value_or(PhysicsEngine::COL_ALL);
		auto hits = m_physics->raycastAll(from.toGlm(), to.toGlm(), mask);
		sol::table result = lua.create_table();
		for (size_t i = 0; i < hits.size(); ++i) {
			sol::table hit = lua.create_table();
			hit["point"] = LuaVec3(hits[i].point);
			hit["normal"] = LuaVec3(hits[i].normal);
			hit["distance"] = hits[i].distance;
			hit["physicsBody"] = hits[i].bodyHandle;
			result[i + 1] = hit;
		}
		return result;
	};

	physics["sphereCast"] = [this](const LuaVec3& from, const LuaVec3& to, float radius, sol::optional<uint32_t> maskOpt) -> sol::object {
		uint32_t mask = maskOpt.value_or(PhysicsEngine::COL_ALL);
		auto hit = m_physics->sphereCast(from.toGlm(), to.toGlm(), radius, mask);
		if (!hit.hit) return sol::nil;
		sol::table result = m_scriptEngine->lua().create_table();
		result["point"] = LuaVec3(hit.point);
		result["normal"] = LuaVec3(hit.normal);
		result["distance"] = hit.distance;
		result["physicsBody"] = hit.bodyHandle;
		return result;
	};

	// --- Взрывы и Дыры ---
	physics["explode"] = [this](const LuaVec3& center, float radius, float strength) {
		m_physics->applyExplosion(center.toGlm(), radius, strength);
	};

	physics["addAttractor"] = [this](const LuaVec3& pos, float radius, float strength) {
		m_physics->addAttractor(pos.toGlm(), radius, strength);
	};

	physics["clearAttractors"] = [this]() {
		m_physics->clearAttractors();
	};

    physics["createCompoundBody"] = [this](sol::object objArg, float mass,
        sol::table children) -> bool
    {
        GameObject* go = resolveObjectArg(objArg);
        if (!go) return false;
        PhysicsBodyComponent* body = getComponent<PhysicsBodyComponent>(*go);
        if (!body) body = &std::get<PhysicsBodyComponent>(go->addComponent(PhysicsBodyComponent{}).data);

        std::vector<PhysicsEngine::CompoundChildData> childrenData;
        std::vector<PhysicsBodyComponent::CompoundChild> childrenSerialized;

        for (auto& [key, value] : children) {
            sol::table child = value;
            PhysicsEngine::CompoundChildData cd;
            PhysicsBodyComponent::CompoundChild sc;

            std::string type = child["type"].get_or(std::string("box"));
            if (type == "sphere")       { cd.shapeType = PhysicsEngine::CollisionShape::SPHERE;      sc.shapeType = 1; }
            else if (type == "capsule") { cd.shapeType = PhysicsEngine::CollisionShape::CAPSULE;     sc.shapeType = 2; }
            else if (type == "cylinder"){ cd.shapeType = PhysicsEngine::CollisionShape::CYLINDER;    sc.shapeType = 3; }
            else if (type == "cone")    { cd.shapeType = PhysicsEngine::CollisionShape::CONE;        sc.shapeType = 6; }
            else if (type == "convex")  { cd.shapeType = PhysicsEngine::CollisionShape::CONVEX_HULL; sc.shapeType = 4; }
            else                        { cd.shapeType = PhysicsEngine::CollisionShape::BOX;         sc.shapeType = 0; }

            LuaVec3 defaultDim(0.5f, 0.5f, 0.5f);
            LuaVec3 defaultOff(0.0f, 0.0f, 0.0f);
            LuaVec3 defaultRot(0.0f, 0.0f, 0.0f);
            LuaVec3 dim = child["dimensions"].get_or(defaultDim);
            LuaVec3 off = child["offset"].get_or(defaultOff);
            LuaVec3 rot = child["rotation"].get_or(defaultRot);

            cd.dimensions = dim.toGlm();
            cd.offset = off.toGlm();
            cd.rotation = rot.toGlm();
            sc.dimensions = dim.toGlm();
            sc.offset = off.toGlm();
            sc.rotation = rot.toGlm();

            if (cd.shapeType == PhysicsEngine::CollisionShape::CONVEX_HULL) {
                std::string meshName = child["mesh"].get_or(std::string(""));
                if (!meshName.empty()) {
                    cd.meshData = getCollisionMeshData(meshName);
                }
            }

            childrenData.push_back(cd);
            childrenSerialized.push_back(sc);
        }

        PhysicsHandle handle = m_physics->createRigidBodyCompound(
            mass, go->position, childrenData, nullptr, glm::quat(glm::radians(go->rotation))
        );

        if (handle != INVALID_HANDLE) {
            body->physicsBody = handle;
            body->physicsEnabled = true;
            body->mass = mass;
            body->shapeType = 8;
            body->compoundChildren = childrenSerialized;
            return true;
        }
        return false;
    };

	physics["loadCollider"] = [this](sol::object objArg,
		const std::string& pecfPath,
		sol::optional<float> massOpt) -> bool
	{
		GameObject* go = resolveObjectArg(objArg);
		if (!go) {
			m_scriptEngine->log("Physics.loadCollider: object not found: " + objectArgName(objArg),
				ConsoleLogEntry::Error);
			return false;
		}
		PhysicsBodyComponent* body = getComponent<PhysicsBodyComponent>(*go);
		if (!body) {
			m_scriptEngine->log("Physics.loadCollider: not a body: " + go->name,
				ConsoleLogEntry::Error);
			return false;
		}
		if (!isPathSafe(pecfPath)) {
			m_scriptEngine->log("Physics.loadCollider: Access denied to path: " + pecfPath,
				ConsoleLogEntry::Error);
			return false;
		}
		if (body->physicsBody != INVALID_HANDLE && m_physics) {
			m_physics->destroyRigidBody(body->physicsBody);
			body->physicsBody = INVALID_HANDLE;
		}
		float mass = massOpt.value_or(body->mass);
		glm::quat initialRot = glm::quat(glm::radians(go->rotation));
		PhysicsHandle handle = m_physics->loadCollider(
			mass, go->position, initialRot, pecfPath, nullptr,
			body->collisionGroup, body->collisionMask
		);
		if (handle == INVALID_HANDLE) {
			m_scriptEngine->log("Physics.loadCollider: failed to load: " + pecfPath,
				ConsoleLogEntry::Error);
			return false;
		}
		body->physicsBody      = handle;
		body->physicsEnabled   = true;
		body->collisionEnabled = true;
		body->mass             = mass;
		body->colliderFile     = pecfPath;
		body->shapeType = static_cast<int>(m_physics->getCollisionShapeType(handle));
		return true;
	};

    physics["saveCollider"] = [this](sol::object objArg,
        const std::string& pecfPath) -> bool
    {
        PhysicsBodyComponent* body = resolveBodyArg(objArg);
        if (!body || body->physicsBody == INVALID_HANDLE) {
            m_scriptEngine->log("Physics.saveCollider: у объекта нет физического тела: "
                + objectArgName(objArg), ConsoleLogEntry::Error);
            return false;
        }
        // saveCollider пишет файл на диск (ofstream + rename), поэтому путь
        // обязан проходить ту же проверку, что и все остальные. Без неё
        // скрипт мог записать что угодно куда угодно мимо песочницы.
        if (!isPathSafe(pecfPath)) {
            m_scriptEngine->log("Physics.saveCollider: Access denied to path: " + pecfPath,
                ConsoleLogEntry::Error);
            return false;
        }
        bool ok = m_physics->saveCollider(body->physicsBody, pecfPath);
        if (ok) body->colliderFile = pecfPath;
        return ok;
    };

    physics["setCollisionMesh"] = [this](sol::object objArg, const std::string& meshName) {
        if (PhysicsBodyComponent* body = resolveBodyArg(objArg))
            body->collisionMeshName = meshName;
    };

    physics["setCollisionEnabled"] = [this](sol::object objArg, bool enabled) {
        if (PhysicsBodyComponent* body = resolveBodyArg(objArg))
            m_physics->setCollisionEnabled(body->physicsBody, enabled);
    };

    physics["setCcd"] = [this](sol::object objArg, float threshold, float radius) {
        PhysicsBodyComponent* body = resolveBodyArg(objArg);
        if (!body || body->physicsBody == INVALID_HANDLE) {
            m_scriptEngine->log("Physics.setCcd: Body not found or no physics: " + objectArgName(objArg),
                ConsoleLogEntry::Warning);
            return;
        }
        m_physics->setCcdParameters(body->physicsBody, threshold, radius);
    };

    physics["destroyBody"] = [this](sol::object objArg) {
        if (PhysicsBodyComponent* body = resolveBodyArg(objArg)) {
            m_physics->destroyRigidBody(body->physicsBody);
            body->physicsBody = INVALID_HANDLE;
            body->physicsEnabled = false;
        }
    };

    physics["hasBody"] = [this](sol::object objArg) -> bool {
        if (PhysicsBodyComponent* body = resolveBodyArg(objArg))
            return body->physicsBody != INVALID_HANDLE;
        return false;
    };

    physics["setVelocity"] = [this](sol::object objArg, const LuaVec3& vel) {
        if (PhysicsBodyComponent* body = resolveBodyArg(objArg))
            m_physics->setLinearVelocity(body->physicsBody, vel.toGlm());
    };

    physics["getVelocity"] = [this](sol::object objArg) -> LuaVec3 {
        if (PhysicsBodyComponent* body = resolveBodyArg(objArg))
            return LuaVec3(m_physics->getLinearVelocity(body->physicsBody));
        return LuaVec3(0, 0, 0);
    };

    physics["setAngularVelocity"] = [this](sol::object objArg, const LuaVec3& vel) {
        if (PhysicsBodyComponent* body = resolveBodyArg(objArg))
            m_physics->setAngularVelocity(body->physicsBody, vel.toGlm());
    };

    physics["getAngularVelocity"] = [this](sol::object objArg) -> LuaVec3 {
        if (PhysicsBodyComponent* body = resolveBodyArg(objArg))
            return LuaVec3(m_physics->getAngularVelocity(body->physicsBody));
        return LuaVec3(0, 0, 0);
    };

    physics["applyImpulse"] = [this](sol::object objArg, const LuaVec3& impulse) {
        if (PhysicsBodyComponent* body = resolveBodyArg(objArg)) {
            m_physics->activate(body->physicsBody);
            glm::vec3 currentVel = m_physics->getLinearVelocity(body->physicsBody);
            float mass = m_physics->getMass(body->physicsBody);
            if (mass > 0)
                m_physics->setLinearVelocity(body->physicsBody, currentVel + impulse.toGlm() / mass);
        }
    };

    physics["setKinematic"] = [this](sol::object objArg, bool kinematic) {
        if (PhysicsBodyComponent* body = resolveBodyArg(objArg))
            m_physics->setKinematic(body->physicsBody, kinematic);
    };

    physics["setPosition"] = [this](sol::object objArg, const LuaVec3& pos) {
        GameObject* go = resolveObjectArg(objArg);
        if (!go) return;
        if (auto* body = getComponent<PhysicsBodyComponent>(*go))
            m_physics->setPositionDirect(body->physicsBody, pos.toGlm());
        go->position = pos.toGlm();
    };

    physics["setRotation"] = [this](sol::object objArg, const LuaVec3& rot) {
        GameObject* go = resolveObjectArg(objArg);
        if (!go) return;
        if (auto* body = getComponent<PhysicsBodyComponent>(*go)) {
            if (body->physicsBody != INVALID_HANDLE && m_physics) {
                glm::mat4 rotMat(1.0f);
                rotMat = glm::rotate(rotMat, glm::radians(rot.toGlm().x), glm::vec3(1, 0, 0));
                rotMat = glm::rotate(rotMat, glm::radians(rot.toGlm().y), glm::vec3(0, 1, 0));
                rotMat = glm::rotate(rotMat, glm::radians(rot.toGlm().z), glm::vec3(0, 0, 1));
                m_physics->setRotation(body->physicsBody, glm::quat_cast(rotMat));
            }
        }
        go->rotation = rot.toGlm();
    };

    physics["setAngularFactor"] = [this](sol::object objArg, const LuaVec3& factor) {
        if (auto* go = resolveObjectArg(objArg)) {
            if (auto* body = getComponent<PhysicsBodyComponent>(*go)) {
                if (body->physicsBody != INVALID_HANDLE)
                    m_physics->setAngularFactor(body->physicsBody, factor.toGlm());
            }
        }
    };

    physics["setGravity"] = [this](sol::object objArg, const LuaVec3& gravity) {
        if (PhysicsBodyComponent* body = resolveBodyArg(objArg))
            m_physics->setGravity(body->physicsBody, gravity.toGlm());
    };

    physics["setWorldGravity"] = [this](const LuaVec3& gravity) {
        m_physics->setWorldGravity(gravity.toGlm());
    };

    physics["getWorldGravity"] = [this]() -> LuaVec3 {
        return LuaVec3(m_physics->getWorldGravity());
    };

    physics["getMass"] = [this](sol::object objArg) -> float {
        if (PhysicsBodyComponent* body = resolveBodyArg(objArg))
            return m_physics->getMass(body->physicsBody);
        return 0.0f;
    };

    physics["setFriction"] = [this](sol::object objArg, float friction) {
        if (PhysicsBodyComponent* body = resolveBodyArg(objArg))
            m_physics->setFriction(body->physicsBody, friction);
    };

    physics["setRestitution"] = [this](sol::object objArg, float restitution) {
        if (PhysicsBodyComponent* body = resolveBodyArg(objArg))
            m_physics->setRestitution(body->physicsBody, restitution);
    };

    physics["activate"] = [this](sol::object objArg) {
        if (PhysicsBodyComponent* body = resolveBodyArg(objArg))
            m_physics->activate(body->physicsBody, true);
    };

    physics["setEnabled"] = [this](sol::object objArg, bool enabled) {
        if (PhysicsBodyComponent* body = resolveBodyArg(objArg))
            m_physics->setBodyActiveInWorld(body->physicsBody, enabled);
    };

    physics["reset"] = [this](sol::object objArg, const LuaVec3& position) {
        if (PhysicsBodyComponent* body = resolveBodyArg(objArg))
            m_physics->resetBodyState(body->physicsBody, position.toGlm());
    };

    physics["getPosition"] = [this](sol::object objArg) -> LuaVec3 {
        if (PhysicsBodyComponent* body = resolveBodyArg(objArg))
            return LuaVec3(m_physics->getPosition(body->physicsBody));
        return LuaVec3(0, 0, 0);
    };

    physics["applyForce"] = [this](sol::object objArg, const LuaVec3& force, sol::optional<LuaVec3> relPos) {
        if (PhysicsBodyComponent* body = resolveBodyArg(objArg)) {
            glm::vec3 rel = relPos.value_or(LuaVec3(0, 0, 0)).toGlm();
            m_physics->applyForce(body->physicsBody, force.toGlm(), rel);
        }
    };

    physics["applyTorque"] = [this](sol::object objArg, const LuaVec3& torque) {
        if (PhysicsBodyComponent* body = resolveBodyArg(objArg))
            m_physics->applyTorque(body->physicsBody, torque.toGlm());
    };

    // ignoreArg — sol::object, а НЕ sol::optional<sol::table>: на nil в этом
    // слоте sol2 не сдвигает индекс аргумента, и привычный вызов
    // Physics.raycast(from, to, nil, mask) молча терял маску групп —
    // луч бил по всем телам, а скрипт считал, что фильтрует.
    physics["raycast"] = [this, &lua](const LuaVec3& from, const LuaVec3& to,
        sol::object ignoreArg,
        sol::optional<uint32_t> groupMaskOpt) -> sol::object
    {
        std::vector<PhysicsHandle> ignoreHandles;

        if (ignoreArg.is<sol::table>()) {
            sol::table ignoreList = ignoreArg.as<sol::table>();
            for (auto& [key, value] : ignoreList) {
                if (value.is<std::string>()) {
                    std::string name = value.as<std::string>();
                    if (PhysicsBodyComponent* body = getBodyByName(name)) {
                        if (body->physicsBody != INVALID_HANDLE)
                            ignoreHandles.push_back(body->physicsBody);
                    }
                } else if (value.is<uint32_t>()) {
                    ignoreHandles.push_back(value.as<uint32_t>());
                } else if (value.is<double>()) {
                    ignoreHandles.push_back(static_cast<uint32_t>(value.as<double>()));
                } else if (value.is<ObjectRef>()) {
                    GameObject* go = value.as<ObjectRef>().get();
                    if (go) {
                        if (auto* body = getComponent<PhysicsBodyComponent>(*go)) {
                            if (body->physicsBody != INVALID_HANDLE)
                                ignoreHandles.push_back(body->physicsBody);
                        }
                    }
                }
            }
        }

        uint32_t groupMask = groupMaskOpt.value_or(PhysicsEngine::COL_ALL);
        PhysicsEngine::RaycastHit hit = m_physics->raycast(from.toGlm(), to.toGlm(), ignoreHandles, groupMask);

        if (!hit.hit) return sol::nil;

        sol::table result = lua.create_table();
        result["hit"] = true;
        result["point"] = LuaVec3(hit.point);
        result["normal"] = LuaVec3(hit.normal);
        result["distance"] = hit.distance;
        result["physicsBody"] = hit.bodyHandle;

        for (auto& go : gameObjects) {
            const auto* body = getComponent<PhysicsBodyComponent>(go);
            if (!body || body->physicsBody != hit.bodyHandle) continue;
            if (const auto* meshComp = getComponent<MeshComponent>(go))
                result["material"] = getMaterialNameByHandle(meshComp->materialHandle);
            result["objectName"] = go.name;
            break;
        }

        return result;
    };

    physics["setCollisionFilter"] = [this](sol::object objArg, uint32_t group, uint32_t mask) {
        if (PhysicsBodyComponent* body = resolveBodyArg(objArg)) {
            if (body->physicsBody != INVALID_HANDLE) {
                PhysicsEngine::CollisionFilter filter;
                filter.group = group;
                filter.mask = mask;
                m_physics->setCollisionFilter(body->physicsBody, filter);
            }
        }
    };

	physics["setScale"] = [this](sol::object objArg, const LuaVec3& scale) {
		GameObject* go = resolveObjectArg(objArg);
		if (!go) return;
		
		// 1. Обновляем визуальный масштаб объекта
		go->scale = scale.toGlm();
		
		// 2. Обновляем физику
		if (auto* body = getComponent<PhysicsBodyComponent>(*go)) {
			if (body->physicsBody != INVALID_HANDLE) {
				m_physics->setLocalScaling(body->physicsBody, go->scale);
			}
			// Синхронизируем логические размеры коллизии (для примитивов это масштаб)
			// Если у тебя collisionDimensions хранит полные размеры, а не scale, адаптируй эту строку:
			body->collisionDimensions = go->scale; 
		}
	};

	// Добавь новую крутую фичу в Lua API:
	physics["setMargin"] = [this](sol::object objArg, float margin) {
		if (auto* body = resolveBodyArg(objArg)) {
			if (body->physicsBody != INVALID_HANDLE) {
				m_physics->setCollisionMargin(body->physicsBody, glm::max(0.0f, margin));
			}
		}
	};

	physics["getMargin"] = [this](sol::object objArg) -> float {
		if (auto* body = resolveBodyArg(objArg)) {
			if (body->physicsBody != INVALID_HANDLE) {
				return m_physics->getCollisionMargin(body->physicsBody);
			}
		}
		return 0.0f;
	};

	physics["getAABB"] = [this](sol::object objArg) -> sol::object {
		if (auto* body = resolveBodyArg(objArg)) {
			if (body->physicsBody != INVALID_HANDLE) {
				BoundingBox aabb = m_physics->getAABB(body->physicsBody);
				if (aabb.isValid()) {
					sol::table result = m_scriptEngine->lua().create_table();
					result["min"] = LuaVec3(aabb.min);
					result["max"] = LuaVec3(aabb.max);
					result["center"] = LuaVec3(aabb.getCenter());
					result["valid"] = true;
					return result;
				}
			}
		}
		sol::table empty = m_scriptEngine->lua().create_table();
		empty["valid"] = false;
		return empty;
	};

    // ===== CONSTRAINTS =====
    sol::table constraints = lua.create_table();
    physics["Constraint"] = constraints;
    lua["Constraint"] = constraints;   // псевдоним для старых скриптов

    constraints["createHinge"] = [this](sol::object objA, sol::object objB,
        const LuaVec3& pivotA, const LuaVec3& pivotB,
        sol::optional<LuaVec3> axisA, sol::optional<LuaVec3> axisB) -> int
    {
        PhysicsBodyComponent* bA = resolveBodyArg(objA);
        PhysicsBodyComponent* bB = resolveBodyArg(objB);
        if (!bA || !bB) return -1;

        glm::vec3 aA = axisA.has_value() ? axisA.value().toGlm() : glm::vec3(0, 0, 1);
        glm::vec3 aB = axisB.has_value() ? axisB.value().toGlm() : glm::vec3(0, 0, 1);

        return static_cast<int>(m_physics->createHingeConstraint(
            bA->physicsBody, bB->physicsBody, pivotA.toGlm(), pivotB.toGlm(), aA, aB));
    };

    constraints["createPoint"] = [this](sol::object objA, sol::object objB,
        const LuaVec3& pivotA, const LuaVec3& pivotB) -> int
    {
        PhysicsBodyComponent* bA = resolveBodyArg(objA);
        PhysicsBodyComponent* bB = resolveBodyArg(objB);
        if (!bA || !bB) return -1;

        return static_cast<int>(m_physics->createPointToPointConstraint(
            bA->physicsBody, bB->physicsBody, pivotA.toGlm(), pivotB.toGlm()));
    };

    constraints["destroy"] = [this](int handle) {
        m_physics->destroyConstraint(static_cast<ConstraintHandle>(handle));
    };

    // ========================================================================
    //  Дополнительные констрейнты
    // ========================================================================
    constraints["createCone"] = [this](sol::object objA, sol::object objB,
        const LuaVec3& point, const LuaVec3& twistAxis, float halfAngle) -> int
    {
        PhysicsBodyComponent* bA = resolveBodyArg(objA);
        PhysicsBodyComponent* bB = resolveBodyArg(objB);
        if (!bA || !bB) return -1;
        return static_cast<int>(m_physics->createConeConstraint(
            bA->physicsBody, bB->physicsBody, point.toGlm(), twistAxis.toGlm(), halfAngle));
    };

    // Сустав регдолла: отдельные пределы на отклонение и на закрутку
    constraints["createSwingTwist"] = [this](sol::object objA, sol::object objB,
        const LuaVec3& point, const LuaVec3& twistAxis, const LuaVec3& planeAxis,
        float normalHalfCone, float planeHalfCone, float twistMin, float twistMax) -> int
    {
        PhysicsBodyComponent* bA = resolveBodyArg(objA);
        PhysicsBodyComponent* bB = resolveBodyArg(objB);
        if (!bA || !bB) return -1;
        return static_cast<int>(m_physics->createSwingTwistConstraint(
            bA->physicsBody, bB->physicsBody, point.toGlm(), twistAxis.toGlm(),
            planeAxis.toGlm(), normalHalfCone, planeHalfCone, twistMin, twistMax));
    };

    constraints["createDistance"] = [this](sol::object objA, sol::object objB,
        const LuaVec3& pointA, const LuaVec3& pointB,
        sol::optional<float> minDist, sol::optional<float> maxDist) -> int
    {
        PhysicsBodyComponent* bA = resolveBodyArg(objA);
        PhysicsBodyComponent* bB = resolveBodyArg(objB);
        if (!bA || !bB) return -1;
        // Без пределов связь жёсткая на текущей длине
        return static_cast<int>(m_physics->createDistanceConstraint(
            bA->physicsBody, bB->physicsBody, pointA.toGlm(), pointB.toGlm(),
            minDist.value_or(-1.0f), maxDist.value_or(-1.0f)));
    };

    constraints["createGear"] = [this](sol::object objA, sol::object objB,
        const LuaVec3& axisA, const LuaVec3& axisB, float ratio) -> int
    {
        PhysicsBodyComponent* bA = resolveBodyArg(objA);
        PhysicsBodyComponent* bB = resolveBodyArg(objB);
        if (!bA || !bB) return -1;
        return static_cast<int>(m_physics->createGearConstraint(
            bA->physicsBody, bB->physicsBody, axisA.toGlm(), axisB.toGlm(), ratio));
    };

    constraints["createPulley"] = [this](sol::object objA, sol::object objB,
        const LuaVec3& bodyPointA, const LuaVec3& bodyPointB,
        const LuaVec3& fixedA, const LuaVec3& fixedB,
        sol::optional<float> ratio, sol::optional<float> minLen, sol::optional<float> maxLen) -> int
    {
        PhysicsBodyComponent* bA = resolveBodyArg(objA);
        PhysicsBodyComponent* bB = resolveBodyArg(objB);
        if (!bA || !bB) return -1;
        return static_cast<int>(m_physics->createPulleyConstraint(
            bA->physicsBody, bB->physicsBody, bodyPointA.toGlm(), bodyPointB.toGlm(),
            fixedA.toGlm(), fixedB.toGlm(),
            ratio.value_or(1.0f), minLen.value_or(0.0f), maxLen.value_or(0.0f)));
    };

    // ========================================================================
    //  CCD
    // ========================================================================
    // Быстрые тела перестают пролетать сквозь тонкие стены. Дороже обычной
    // проверки, поэтому включается точечно — пулям и брошенным предметам.
    physics["setContinuousCollision"] = [this](sol::object objArg, bool enabled) -> bool {
        PhysicsBodyComponent* body = resolveBodyArg(objArg);
        if (!body || body->physicsBody == INVALID_HANDLE) return false;
        m_physics->setContinuousCollision(body->physicsBody, enabled);
        return true;
    };
    physics["getContinuousCollision"] = [this](sol::object objArg) -> bool {
        PhysicsBodyComponent* body = resolveBodyArg(objArg);
        if (!body || body->physicsBody == INVALID_HANDLE) return false;
        return m_physics->getContinuousCollision(body->physicsBody);
    };

    // ========================================================================
    //  Контроллер персонажа
    // ========================================================================
    sol::table character = lua.create_table();
    physics["Character"] = character;
    lua["Character"] = character;   // псевдоним для старых скриптов

    character["create"] = [this](const LuaVec3& position, sol::optional<sol::table> opts) -> int {
        PhysicsEngine::CharacterSettings s;
        if (opts) {
            sol::table t = *opts;
            s.radius        = t.get_or("radius", s.radius);
            s.height        = t.get_or("height", s.height);
            s.mass          = t.get_or("mass", s.mass);
            s.maxSlopeAngle = t.get_or("maxSlopeAngle", s.maxSlopeAngle);
            s.stepUp        = t.get_or("stepUp", s.stepUp);
            s.stepDown      = t.get_or("stepDown", s.stepDown);
            s.group         = t.get_or("group", s.group);
            s.mask          = t.get_or("mask", s.mask);
            s.applyGravity  = t.get_or("applyGravity", s.applyGravity);
        }
        return static_cast<int>(m_physics->createCharacter(position.toGlm(), s));
    };

    character["destroy"] = [this](int handle) {
        m_physics->destroyCharacter(static_cast<CharacterHandle>(handle));
    };
    character["isValid"] = [this](int handle) -> bool {
        return m_physics->isValidCharacter(static_cast<CharacterHandle>(handle));
    };
    character["setVelocity"] = [this](int handle, const LuaVec3& v) {
        m_physics->characterSetLinearVelocity(static_cast<CharacterHandle>(handle), v.toGlm());
    };
    character["getVelocity"] = [this](int handle) -> LuaVec3 {
        return LuaVec3(m_physics->characterGetLinearVelocity(static_cast<CharacterHandle>(handle)));
    };
    character["getPosition"] = [this](int handle) -> LuaVec3 {
        return LuaVec3(m_physics->characterGetPosition(static_cast<CharacterHandle>(handle)));
    };
    character["setPosition"] = [this](int handle, const LuaVec3& p) {
        m_physics->characterSetPosition(static_cast<CharacterHandle>(handle), p.toGlm());
    };
    character["getRotation"] = [this](int handle) -> LuaVec3 {
        const glm::quat q = m_physics->characterGetRotation(static_cast<CharacterHandle>(handle));
        return LuaVec3(glm::degrees(glm::eulerAngles(q)));
    };
    character["setRotation"] = [this](int handle, const LuaVec3& euler) {
        m_physics->characterSetRotation(static_cast<CharacterHandle>(handle),
                                        glm::quat(glm::radians(euler.toGlm())));
    };
    // Строка, а не число: "onGround" читается в скрипте лучше, чем 0
    character["groundState"] = [this](int handle) -> std::string {
        switch (m_physics->characterGetGroundState(static_cast<CharacterHandle>(handle))) {
            case PhysicsEngine::GroundState::OnGround:      return "onGround";
            case PhysicsEngine::GroundState::OnSteepGround: return "onSteepGround";
            case PhysicsEngine::GroundState::NotSupported:  return "notSupported";
            default:                                        return "inAir";
        }
    };
    // Отдельно от groundState: стоя на слишком крутом склоне персонаж
    // «на земле», но прыгать ему нельзя
    character["isOnGround"] = [this](int handle) -> bool {
        return m_physics->characterGetGroundState(static_cast<CharacterHandle>(handle))
               == PhysicsEngine::GroundState::OnGround;
    };
    character["groundNormal"] = [this](int handle) -> LuaVec3 {
        return LuaVec3(m_physics->characterGetGroundNormal(static_cast<CharacterHandle>(handle)));
    };
    // Имя объекта, на котором стоит персонаж (движущаяся платформа), или ""
    character["groundObject"] = [this](int handle) -> std::string {
        const PhysicsHandle ground =
            m_physics->characterGetGroundBody(static_cast<CharacterHandle>(handle));
        if (ground == INVALID_HANDLE) return "";
        for (const auto& go : gameObjects) {
            const auto* body = getComponent<PhysicsBodyComponent>(go);
            if (body && body->physicsBody == ground) return go.name;
        }
        return "";
    };

    // ========================================================================
    //  Регдоллы
    // ========================================================================
    sol::table ragdoll = lua.create_table();
    physics["Ragdoll"] = ragdoll;
    lua["Ragdoll"] = ragdoll;   // псевдоним для старых скриптов

    // Части описываются таблицей: {name, parent, shape, dimensions, position,
    // rotation, mass, swingLimit, twistMin, twistMax, jointPoint}.
    // parent — номер части в этом же списке (с 1, как принято в Lua), 0 — корень.
    ragdoll["create"] = [this](sol::table partList, sol::optional<sol::table> opts) -> int {
        std::vector<PhysicsEngine::RagdollPart> parts;
        parts.reserve(partList.size());

        for (size_t i = 1; i <= partList.size(); ++i) {
            sol::optional<sol::table> entry = partList[i];
            if (!entry) continue;
            sol::table t = *entry;

            PhysicsEngine::RagdollPart part;
            part.name = t.get_or("name", std::string("part") + std::to_string(i));
            // В Lua индексы с единицы, внутри — с нуля
            part.parent = t.get_or("parent", 0) - 1;
            part.mass = t.get_or("mass", 1.0f);
            part.swingLimitDegrees = t.get_or("swingLimit", 45.0f);
            part.twistMinDegrees = t.get_or("twistMin", -30.0f);
            part.twistMaxDegrees = t.get_or("twistMax", 30.0f);

            const std::string shape = t.get_or("shape", std::string("capsule"));
            if      (shape == "box")      part.shape = PhysicsEngine::CollisionShape::BOX;
            else if (shape == "sphere")   part.shape = PhysicsEngine::CollisionShape::SPHERE;
            else if (shape == "cylinder") part.shape = PhysicsEngine::CollisionShape::CYLINDER;
            else                          part.shape = PhysicsEngine::CollisionShape::CAPSULE;

            if (sol::optional<LuaVec3> d = t["dimensions"]) part.dimensions = d->toGlm();
            if (sol::optional<LuaVec3> p = t["position"])   part.position   = p->toGlm();
            if (sol::optional<LuaVec3> r = t["rotation"])   part.rotation   = r->toGlm();
            if (sol::optional<LuaVec3> j = t["jointPoint"]) {
                part.jointPoint = j->toGlm();
                part.hasJointPoint = true;
            }
            parts.push_back(std::move(part));
        }

        uint32_t group = PhysicsEngine::COL_DYNAMIC;
        uint32_t mask = PhysicsEngine::COL_ALL;
        if (opts) {
            group = opts->get_or("group", group);
            mask  = opts->get_or("mask", mask);
        }
        return static_cast<int>(m_physics->createRagdoll(parts, group, mask));
    };

    // Привязывает части регдолла к костям скелета: с этого момента скиннингованный
    // меш повторяет движение физики. mapping — таблица {имя_части = имя_кости};
    // если её не передать, части ищутся по СОВПАДЕНИЮ ИМЕНИ с костью.
    // Возвращает число привязанных костей.
    ragdoll["bindToSkeleton"] = [this](int handle, sol::object objArg,
                                       sol::object mappingArg) -> int {
        GameObject* go = resolveObjectArg(objArg);
        if (!go) {
            m_scriptEngine->log("Ragdoll.bindToSkeleton: объект не найден", ConsoleLogEntry::Error);
            return 0;
        }
        auto* skel = getComponent<SkeletonComponent>(*go);
        if (!skel) {
            m_scriptEngine->log("Ragdoll.bindToSkeleton: у объекта нет скелета",
                                ConsoleLogEntry::Error);
            return 0;
        }

        const auto rag = static_cast<RagdollHandle>(handle);
        const size_t parts = m_physics->ragdollPartCount(rag);
        if (parts == 0) {
            m_scriptEngine->log("Ragdoll.bindToSkeleton: у регдолла нет частей",
                                ConsoleLogEntry::Error);
            return 0;
        }

        sol::table mapping;
        const bool hasMapping = mappingArg.is<sol::table>();
        if (hasMapping) mapping = mappingArg.as<sol::table>();

        int bound = 0;
        for (size_t i = 0; i < parts; ++i) {
            const std::string partName = m_physics->ragdollPartName(rag, i);
            std::string boneName = partName;
            if (hasMapping) {
                sol::optional<std::string> mapped = mapping[partName];
                if (!mapped) continue;      // часть намеренно не привязывают
                boneName = *mapped;
            }
            const PhysicsHandle body = m_physics->ragdollPartBody(rag, i);
            if (body == INVALID_HANDLE) continue;
            if (driveBoneFromBody(*go, *skel, boneName, body)) ++bound;
            else if (hasMapping)
                m_scriptEngine->log("Ragdoll.bindToSkeleton: кости '" + boneName + "' нет",
                                    ConsoleLogEntry::Warning);
        }
        return bound;
    };

    // Отпускает все кости, привязанные к этому регдоллу
    ragdoll["unbindSkeleton"] = [this](int handle, sol::object objArg) -> int {
        GameObject* go = resolveObjectArg(objArg);
        if (!go) return 0;
        auto* skel = getComponent<SkeletonComponent>(*go);
        if (!skel) return 0;

        const auto rag = static_cast<RagdollHandle>(handle);
        const size_t parts = m_physics->ragdollPartCount(rag);
        int released = 0;
        for (size_t i = 0; i < parts; ++i) {
            const PhysicsHandle body = m_physics->ragdollPartBody(rag, i);
            for (auto& bone : skel->bones)
                if (bone.drivingBody == body) {
                    bone.drivingBody = INVALID_HANDLE;
                    bone.bodyToBone = glm::mat4(1.0f);
                    ++released;
                }
        }
        if (released > 0) skel->isDirty = true;
        return released;
    };

    ragdoll["destroy"] = [this](int handle) {
        m_physics->destroyRagdoll(static_cast<RagdollHandle>(handle));
    };
    ragdoll["isValid"] = [this](int handle) -> bool {
        return m_physics->isValidRagdoll(static_cast<RagdollHandle>(handle));
    };
    ragdoll["partCount"] = [this](int handle) -> int {
        return static_cast<int>(m_physics->ragdollPartCount(static_cast<RagdollHandle>(handle)));
    };
    ragdoll["activate"] = [this](int handle) {
        m_physics->ragdollActivate(static_cast<RagdollHandle>(handle));
    };
    ragdoll["sleep"] = [this](int handle) {
        m_physics->ragdollSleep(static_cast<RagdollHandle>(handle));
    };
    // Мировая трансформация части: по ней двигают меш руки или ноги
    ragdoll["partPosition"] = [this](int handle, int index) -> LuaVec3 {
        const PhysicsHandle body =
            m_physics->ragdollPartBody(static_cast<RagdollHandle>(handle), size_t(index - 1));
        if (body == INVALID_HANDLE) return LuaVec3(0);
        return LuaVec3(m_physics->getPosition(body));
    };
    ragdoll["partRotation"] = [this](int handle, int index) -> LuaVec3 {
        const PhysicsHandle body =
            m_physics->ragdollPartBody(static_cast<RagdollHandle>(handle), size_t(index - 1));
        if (body == INVALID_HANDLE) return LuaVec3(0);
        return LuaVec3(glm::degrees(glm::eulerAngles(m_physics->getRotation(body))));
    };
    // Привязать часть регдолла к существующему объекту сцены: его меш будет
    // ездить за телом части. Так и «оживляют» персонажа после смерти.
    ragdoll["attachPart"] = [this](int handle, int index, sol::object objArg) -> bool {
        GameObject* go = resolveObjectArg(objArg);
        if (!go) return false;
        const PhysicsHandle body =
            m_physics->ragdollPartBody(static_cast<RagdollHandle>(handle), size_t(index - 1));
        if (body == INVALID_HANDLE) return false;

        auto* comp = getComponent<PhysicsBodyComponent>(*go);
        if (!comp) comp = &std::get<PhysicsBodyComponent>(go->addComponent(PhysicsBodyComponent{}).data);
        comp->physicsBody = body;
        comp->physicsEnabled = true;
        comp->collisionEnabled = true;
        comp->mass = m_physics->getMass(body);
        return true;
    };

    // ========================================================================
    //  Мягкие тела и ткань
    // ========================================================================
    sol::table soft = lua.create_table();
    physics["SoftBody"] = soft;
    lua["SoftBody"] = soft;   // псевдоним для старых скриптов

    auto readSoftSettings = [](sol::optional<sol::table> opts) {
        PhysicsEngine::SoftBodySettings s;
        if (opts) {
            sol::table t = *opts;
            s.mass          = t.get_or("mass", s.mass);
            s.compliance    = t.get_or("compliance", s.compliance);
            s.pressure      = t.get_or("pressure", s.pressure);
            s.friction      = t.get_or("friction", s.friction);
            s.restitution   = t.get_or("restitution", s.restitution);
            s.linearDamping = t.get_or("damping", s.linearDamping);
            s.numIterations = t.get_or("iterations", s.numIterations);
            s.group         = t.get_or("group", s.group);
            s.mask          = t.get_or("mask", s.mask);
        }
        return s;
    };

    // Ткань: сетка в плоскости XY, pinned — номера приколоченных вершин
    // (индекс = y * (segmentsX + 1) + x, с единицы как принято в Lua)
    // optsArg/pinnedArg — sol::object по той же причине, что и в raycast:
    // nil в слоте таблицы не сдвигал индекс, и createCloth(..., nil, {1,2})
    // терял список закреплённых вершин
    soft["createCloth"] = [this, readSoftSettings](const LuaVec3& origin, const LuaVec3& rotationEuler,
        float sizeX, float sizeY, int segX, int segY,
        sol::object optsArg, sol::object pinnedArg) -> int
    {
        std::vector<int> pinned;
        if (pinnedArg.is<sol::table>()) {
            sol::table pinnedList = pinnedArg.as<sol::table>();
            for (size_t i = 1; i <= pinnedList.size(); ++i) {
                sol::optional<int> idx = pinnedList[i];
                if (idx) pinned.push_back(*idx - 1);
            }
        }
        const glm::quat rot = glm::quat(glm::radians(rotationEuler.toGlm()));
        return static_cast<int>(m_physics->createCloth(origin.toGlm(), rot, sizeX, sizeY,
                                                       segX, segY,
                                                       readSoftSettings(optsArg.is<sol::table>()
                                                           ? sol::optional<sol::table>(optsArg.as<sol::table>())
                                                           : sol::nullopt),
                                                       pinned));
    };

    // Мягкое тело из меша ресурса: берём его вершины и треугольники
    soft["createFromMesh"] = [this, readSoftSettings](const std::string& meshName,
        const LuaVec3& position, const LuaVec3& rotationEuler,
        sol::optional<sol::table> opts) -> int
    {
        const MeshHandle mesh = getMeshHandleByName(meshName);
        if (mesh == INVALID_HANDLE) {
            m_scriptEngine->log("SoftBody.createFromMesh: меша '" + meshName + "' нет",
                                ConsoleLogEntry::Error);
            return -1;
        }
        auto data = graphics->getMeshVertexData(mesh);
        if (data.vertices.empty()) return -1;

        PhysicsEngine::CollisionMeshData collision;
        collision.vertices = data.vertices;   // уже готовые позиции
        collision.indices = data.indices;

        const glm::quat rot = glm::quat(glm::radians(rotationEuler.toGlm()));
        return static_cast<int>(m_physics->createSoftBody(collision, position.toGlm(), rot,
                                                          readSoftSettings(opts)));
    };

    // ------------------------------------------------------------------
    // Деформация визуального меша мягким телом
    // ------------------------------------------------------------------
    // Главный сценарий: берём меш объекта, делаем из него мягкое тело и
    // привязываем деформацию обратно к этому же мешу. Дальше движок каждый
    // кадр сам переносит позиции частиц в вершины (фаза 6.5).
    //
    // Возвращает хендл мягкого тела или -1. Меш объекта заменяется на его
    // ПЕРСОНАЛЬНУЮ копию: деформация общего ресурса поехала бы по всем
    // объектам с тем же мешем.
	soft["createFromObject"] = [this, readSoftSettings](sol::object objArg,
		sol::optional<sol::table> opts, sol::object pinnedArg) -> int
	{
		GameObject* go = resolveObjectArg(objArg);
        if (!go) {
            m_scriptEngine->log("SoftBody.createFromObject: объект не найден", ConsoleLogEntry::Error);
            return -1;
        }
		
	    std::vector<int> pinned;
		if (pinnedArg.is<sol::table>()) {
			sol::table pinnedList = pinnedArg.as<sol::table>();
			for (size_t i = 1; i <= pinnedList.size(); ++i) {
				sol::optional<int> idx = pinnedList[i];
				if (idx) pinned.push_back(*idx - 1); // В Lua индексы с 1, в C++ с 0
			}
		}
		
        Component* meshComp = nullptr;
        for (auto& c : go->components)
            if (std::holds_alternative<MeshComponent>(c.data)) { meshComp = &c; break; }
        if (!meshComp) {
            m_scriptEngine->log("SoftBody.createFromObject: у объекта нет меша", ConsoleLogEntry::Error);
            return -1;
        }
        auto* mesh = std::get_if<MeshComponent>(&meshComp->data);
        if (mesh->renderable == INVALID_HANDLE) {
            m_scriptEngine->log("SoftBody.createFromObject: меш не назначен", ConsoleLogEntry::Error);
            return -1;
        }
        if (mesh->softBodyHandle != 0) {
            m_scriptEngine->log("SoftBody.createFromObject: у меша уже есть мягкое тело",
                                ConsoleLogEntry::Warning);
            return -1;
        }

        // Копия меша под запись + сваренная геометрия для физики
        GraphicsEngineGL::WeldedMeshData welded;
        const MeshHandle copy = graphics->createDeformableMesh(mesh->renderable, welded);
        if (copy == INVALID_HANDLE || welded.positions.empty()) {
            m_scriptEngine->log("SoftBody.createFromObject: не удалось подготовить меш",
                                ConsoleLogEntry::Error);
            return -1;
        }

        go->updateComponentTransforms();

        // Частицы физика хранит в мире, поэтому масштаб объекта запекаем
        // в позиции: у мягкого тела своего масштаба нет.
        PhysicsEngine::CollisionMeshData collision;
        collision.vertices.reserve(welded.positions.size());
        for (const glm::vec3& p : welded.positions) collision.vertices.push_back(p * go->scale);
        collision.indices = welded.indices;

		const PhysicsHandle body = m_physics->createSoftBody(
			collision, meshComp->worldPosition, meshComp->worldRotationQuat,
			readSoftSettings(opts), pinned);
        if (body == INVALID_HANDLE) {
            m_scriptEngine->log("SoftBody.createFromObject: физика отказалась создавать тело",
                                ConsoleLogEntry::Error);
            return -1;
        }

        mesh->sourceRenderable = mesh->renderable;
        mesh->renderable = copy;
        mesh->softBodyHandle = static_cast<uint32_t>(body);
        return static_cast<int>(body);
    };

    // Отвязывает деформацию и возвращает объекту исходный меш.
    // Само мягкое тело НЕ удаляется — за него отвечает SoftBody.destroy.
    soft["unbindMesh"] = [this](sol::object objArg) -> bool {
        GameObject* go = resolveObjectArg(objArg);
        if (!go) return false;
        for (auto& c : go->components) {
            auto* mesh = std::get_if<MeshComponent>(&c.data);
            if (!mesh || mesh->softBodyHandle == 0) continue;
            if (mesh->sourceRenderable != INVALID_HANDLE) mesh->renderable = mesh->sourceRenderable;
            mesh->sourceRenderable = INVALID_HANDLE;
            mesh->softBodyHandle = 0;
            return true;
        }
        return false;
    };

    // Привязано ли к мешу объекта мягкое тело (0 — нет)
    soft["meshBinding"] = [this](sol::object objArg) -> int {
        GameObject* go = resolveObjectArg(objArg);
        if (!go) return 0;
        for (auto& c : go->components)
            if (auto* mesh = std::get_if<MeshComponent>(&c.data))
                if (mesh->softBodyHandle != 0) return static_cast<int>(mesh->softBodyHandle);
        return 0;
    };

    soft["destroy"] = [this](int handle) {
        // Снимаем привязки к этому телу, иначе фаза 6.5 продолжит читать
        // удалённое тело и меш замрёт на последней позе
        for (auto& go : gameObjects)
            for (auto& c : go.components)
                if (auto* mesh = std::get_if<MeshComponent>(&c.data))
                    if (mesh->softBodyHandle == static_cast<uint32_t>(handle)) {
                        if (mesh->sourceRenderable != INVALID_HANDLE)
                            mesh->renderable = mesh->sourceRenderable;
                        mesh->sourceRenderable = INVALID_HANDLE;
                        mesh->softBodyHandle = 0;
                    }
        m_physics->destroyRigidBody(static_cast<PhysicsHandle>(handle));
    };
    soft["vertexCount"] = [this](int handle) -> int {
        return static_cast<int>(m_physics->getSoftBodyVertices(static_cast<PhysicsHandle>(handle)).size());
    };
    // Мировые позиции вершин — их отдают в рендер, чтобы нарисовать ткань
    soft["getVertices"] = [this](int handle) -> sol::table {
        sol::table out = m_scriptEngine->lua().create_table();
        const auto verts = m_physics->getSoftBodyVertices(static_cast<PhysicsHandle>(handle));
        for (size_t i = 0; i < verts.size(); ++i) out[i + 1] = LuaVec3(verts[i]);
        return out;
    };
    soft["getVertex"] = [this](int handle, int index) -> LuaVec3 {
        const auto verts = m_physics->getSoftBodyVertices(static_cast<PhysicsHandle>(handle));
        if (index < 1 || index > static_cast<int>(verts.size())) return LuaVec3(0);
        return LuaVec3(verts[index - 1]);
    };

    // ========================================================================
    //  Транспорт
    // ========================================================================
    sol::table vehicle = lua.create_table();
    physics["Vehicle"] = vehicle;
    lua["Vehicle"] = vehicle;   // псевдоним для старых скриптов

    // Колёса — список таблиц {position, radius, width, steer, driven,
    // handBrake, suspensionMin, suspensionMax}
    vehicle["create"] = [this](sol::object bodyArg, sol::table wheelList,
        sol::optional<sol::table> opts) -> int
    {
        PhysicsBodyComponent* body = resolveBodyArg(bodyArg);
        if (!body || body->physicsBody == INVALID_HANDLE) {
            m_scriptEngine->log("Vehicle.create: у объекта нет физического тела",
                                ConsoleLogEntry::Error);
            return -1;
        }

        std::vector<PhysicsEngine::WheelDesc> wheels;
        for (size_t i = 1; i <= wheelList.size(); ++i) {
            sol::optional<sol::table> entry = wheelList[i];
            if (!entry) continue;
            sol::table t = *entry;

            PhysicsEngine::WheelDesc w;
            if (sol::optional<LuaVec3> p = t["position"]) w.position = p->toGlm();
            w.radius = t.get_or("radius", w.radius);
            w.width  = t.get_or("width", w.width);
            w.suspensionMinLength = t.get_or("suspensionMin", w.suspensionMinLength);
            w.suspensionMaxLength = t.get_or("suspensionMax", w.suspensionMaxLength);
            w.suspensionFrequency = t.get_or("suspensionFrequency", w.suspensionFrequency);
            w.suspensionDamping   = t.get_or("suspensionDamping", w.suspensionDamping);
            w.maxSteerAngleDegrees = t.get_or("steer", w.maxSteerAngleDegrees);
            w.driven = t.get_or("driven", w.driven);
            w.maxHandBrakeTorque = t.get_or("handBrake", w.maxHandBrakeTorque);
            wheels.push_back(w);
        }

        PhysicsEngine::VehicleSettings s;
        if (opts) {
            sol::table t = *opts;
            s.maxEngineTorque = t.get_or("engineTorque", s.maxEngineTorque);
            s.maxEngineRPM    = t.get_or("engineRPM", s.maxEngineRPM);
            s.maxBrakeTorque  = t.get_or("brakeTorque", s.maxBrakeTorque);
            if (sol::optional<LuaVec3> f = t["forward"]) s.forward = f->toGlm();
            if (sol::optional<LuaVec3> u = t["up"])      s.up = u->toGlm();
        }
        return static_cast<int>(m_physics->createVehicle(body->physicsBody, wheels, s));
    };

    vehicle["destroy"] = [this](int handle) {
        m_physics->destroyVehicle(static_cast<VehicleHandle>(handle));
    };
    vehicle["isValid"] = [this](int handle) -> bool {
        return m_physics->isValidVehicle(static_cast<VehicleHandle>(handle));
    };
    // forward и right в [-1..1], brake и handBrake в [0..1]
    vehicle["setInput"] = [this](int handle, float forward, float right,
                                 sol::optional<float> brake, sol::optional<float> handBrake) {
        m_physics->vehicleSetInput(static_cast<VehicleHandle>(handle), forward, right,
                                   brake.value_or(0.0f), handBrake.value_or(0.0f));
    };
    vehicle["wheelCount"] = [this](int handle) -> int {
        return static_cast<int>(m_physics->vehicleWheelCount(static_cast<VehicleHandle>(handle)));
    };
    vehicle["wheelPosition"] = [this](int handle, int wheel) -> LuaVec3 {
        const glm::mat4 t = m_physics->vehicleWheelTransform(static_cast<VehicleHandle>(handle),
                                                             size_t(wheel - 1));
        return LuaVec3(glm::vec3(t[3]));
    };
    vehicle["wheelRotation"] = [this](int handle, int wheel) -> LuaVec3 {
        const glm::mat4 t = m_physics->vehicleWheelTransform(static_cast<VehicleHandle>(handle),
                                                             size_t(wheel - 1));
        // Масштаба у колеса нет, поэтому линейная часть — чистый поворот
        return LuaVec3(glm::degrees(glm::eulerAngles(glm::quat_cast(glm::mat3(t)))));
    };
}


bool SceneEditorApp::setMaterialParam(const std::string& name, const std::string& param, sol::object value) {
	MaterialHandle handle = graphics->getMaterialHandleByName(name);
    if (handle == INVALID_HANDLE) {
        m_scriptEngine->log("Material not found: " + name, ConsoleLogEntry::Error);
        return false;
    }
    Material mat = graphics->getMaterial(handle);
    bool changed = false;

    // --- Существующие параметры ---
    if (param == "metallic") { mat.metallic = glm::clamp(static_cast<float>(value.as<double>()), 0.0f, 1.0f); changed = true; }
    else if (param == "roughness") { mat.roughness = glm::clamp(static_cast<float>(value.as<double>()), 0.0f, 1.0f); changed = true; }
    else if (param == "ao") { mat.ao = glm::clamp(static_cast<float>(value.as<double>()), 0.0f, 1.0f); changed = true; }
    else if (param == "uvScale") { mat.uvScale = std::max(static_cast<float>(value.as<double>()), 0.01f); changed = true; }
    else if (param == "uvOffset_x") { mat.uvOffset.x = static_cast<float>(value.as<double>()); changed = true; }
    else if (param == "uvOffset_y") { mat.uvOffset.y = static_cast<float>(value.as<double>()); changed = true; }
    else if (param == "flags") { mat.flags = static_cast<uint32_t>(value.as<double>()); changed = true; }
	else if (param == "filtering") { mat.filteringType = static_cast<MaterialFiltering>(value.as<int>()); changed = true; }
    // Номер шейдерной программы материала (см. "shaderprograms" в cfg.json)
    else if (param == "shader") {
        const int index = static_cast<int>(value.as<double>());
        if (index < 0 || index > 255) {
            m_scriptEngine->log("setMaterialParam: номер шейдера вне 0..255", ConsoleLogEntry::Error);
            return false;
        }
        if (!graphics->isShaderProgramValid(static_cast<uint8_t>(index)))
            m_scriptEngine->log("setMaterialParam: шейдерная программа " + std::to_string(index) +
                                " не собрана, материал будет рисоваться запасной",
                                ConsoleLogEntry::Warning);
        mat.shaderIndex = static_cast<uint8_t>(index);
        changed = true;
    }
    else if (param == "isDistortion") {
        mat.isDistortion = value.as<bool>();
        changed = true;
    }
	// --- НОВЫЕ КАСТОМНЫЕ ПАРАМЕТРЫ ---
    else if (param == "customType") {
        // Совместимость со старыми скриптами: раньше тип материала выбирался
        // именно так, теперь это отдельная шейдерная программа
        mat.customType = static_cast<uint32_t>(value.as<double>());
        if (mat.customType >= 1 && mat.customType <= 15) {
            mat.shaderIndex = SHADER_WATER;
            mat.isDistortion = (mat.customType >= 2);
        } else if (mat.customType == 0) {
            // Сброс должен работать в обе стороны: раньше ветвление жило внутри
            // pbr.frag и откат получался сам собой
            mat.shaderIndex = SHADER_SOLID;
            mat.isDistortion = false;
        }
        changed = true;
    }
    else if (param == "customParam1") {
        if (value.is<LuaVec4>()) { mat.customParam1 = value.as<LuaVec4>().toGlm(); changed = true; }
        else return false;
    }
    else if (param == "customParam2") {
        if (value.is<LuaVec4>()) { mat.customParam2 = glm::ivec4(value.as<LuaVec4>().toGlm()); changed = true; }
		else return false;
    }
    else {
        m_scriptEngine->log("Unknown material parameter: " + param, ConsoleLogEntry::Warning);
        return false;
    }

    if (changed) {
        graphics->updateMaterial(handle, mat);
    }
    return changed;
}


sol::object SceneEditorApp::getMaterialParam(const std::string& name, const std::string& param, sol::this_state s) {
    sol::state_view lua(s);
    MaterialHandle handle = graphics->getMaterialHandleByName(name);
    if (handle == INVALID_HANDLE) return sol::nil;
    Material mat = graphics->getMaterial(handle);

    if (param == "metallic") return sol::make_object(lua, mat.metallic);
    if (param == "roughness") return sol::make_object(lua, mat.roughness);
    if (param == "ao") return sol::make_object(lua, mat.ao);
    if (param == "uvScale") return sol::make_object(lua, mat.uvScale);
    if (param == "uvOffset_x") return sol::make_object(lua, mat.uvOffset.x);
    if (param == "uvOffset_y") return sol::make_object(lua, mat.uvOffset.y);
    if (param == "flags") return sol::make_object(lua, static_cast<double>(mat.flags));
	if (param == "filtering") return sol::make_object(lua, static_cast<int>(mat.filteringType));
    if (param == "shader") return sol::make_object(lua, static_cast<int>(mat.shaderIndex));
    if (param == "isDistortion") return sol::make_object(lua, mat.isDistortion);
    
    // --- НОВЫЕ КАСТОМНЫЕ ПАРАМЕТРЫ ---
    if (param == "customType") return sol::make_object(lua, static_cast<double>(mat.customType));
    if (param == "customParam1") return sol::make_object(lua, LuaVec4(mat.customParam1));
    if (param == "customParam2") return sol::make_object(lua, LuaVec4(glm::vec4(mat.customParam2))); // Каст для Lua

    return sol::nil;
}


GameObject* SceneEditorApp::resolveObjectArg(sol::object obj) {
    if (obj.is<std::string>()) return getObjectByName(obj.as<std::string>());
    if (obj.is<ObjectRef>())   return obj.as<ObjectRef>().get();
    if (obj.is<uint32_t>())    return getObjectById(obj.as<uint32_t>());
    if (obj.is<double>())      return getObjectById(static_cast<uint32_t>(obj.as<double>()));
    return nullptr;
}


PhysicsBodyComponent* SceneEditorApp::resolveBodyArg(sol::object obj) {
    GameObject* go = resolveObjectArg(obj);
    return go ? getComponent<PhysicsBodyComponent>(*go) : nullptr;
}


std::string SceneEditorApp::objectArgName(sol::object obj) {
    if (obj.is<std::string>()) return obj.as<std::string>();
    if (obj.is<ObjectRef>()) {
        GameObject* go = obj.as<ObjectRef>().get();
        return go ? go->name : "ref:" + std::to_string(obj.as<ObjectRef>().id);
    }
    if (obj.is<uint32_t>()) return "id:" + std::to_string(obj.as<uint32_t>());
    if (obj.is<double>())   return "id:" + std::to_string(static_cast<uint32_t>(obj.as<double>()));
    return "<invalid>";
}
