// ScriptBindingsObject.cpp
// Регистрация типа ObjectRef — Lua-обёртки над GameObject. Здесь живут все
// свойства компонентов (тело, свет, звук, партиклы, анимация, скелет) и методы
// работы с иерархией.
//
// Вынесено из ScriptBindings.cpp отдельно: один usertype со ~150 свойствами
// разворачивается sol2 в огромное дерево шаблонов, и вместе с остальными
// таблицами компилятору не хватает памяти.
#include "main.h"

void SceneEditorApp::bindObjectAPI() {
    sol::state& lua = m_scriptEngine->lua();

	// === ENUMS REGISTRATION ===
	lua.new_enum("ParticleBlendMode",
		"Additive", static_cast<int>(ParticleBlendMode::Additive),
		"AlphaBlend", static_cast<int>(ParticleBlendMode::AlphaBlend),
		"Distortion", static_cast<int>(ParticleBlendMode::Distortion)
	);

	lua.new_enum("ParticleOrientation",
		"FacingCamera", static_cast<int>(ParticleOrientation::FacingCamera),
		"FacingCameraWorldUp", static_cast<int>(ParticleOrientation::FacingCameraWorldUp),
		"VelocityParallel", static_cast<int>(ParticleOrientation::VelocityParallel),
		"VelocityPerpendicular", static_cast<int>(ParticleOrientation::VelocityPerpendicular),
		"Directed", static_cast<int>(ParticleOrientation::Directed)
	);

	lua.new_enum("LightType",
		"Point", static_cast<int>(LightType::Point),
		"Directional", static_cast<int>(LightType::Directional),
		"Spot", static_cast<int>(LightType::Spot)
	);

	lua.new_enum("ConstraintType",
		"PointToPoint", ConstraintComponent::PointToPoint,
		"Hinge", ConstraintComponent::Hinge,
		"Slider", ConstraintComponent::Slider,
		"Fixed", ConstraintComponent::Fixed,
		"Spring", ConstraintComponent::Spring,
		"Cable", ConstraintComponent::Cable
	);

	lua.new_enum("TonemapOperator",
		"Reinhard", 0, "Extended", 1, "Filmic", 2, "ACES", 3, "Uchimura", 4
	);

	lua.new_enum("GizmoOperation",
		"Translate", 0, "Rotate", 1, "Scale", 2
	);

	lua.new_enum("GizmoMode",
		"Local", 0, "World", 1
	);

	lua.new_enum("InputMode",
		"Editor", static_cast<int>(InputMode::EDITOR_MODE),
		"Game", static_cast<int>(InputMode::GAME_MODE),
		"None", static_cast<int>(InputMode::NONE_MODE),
		"NoneNoCursor", static_cast<int>(InputMode::NONE_NOCURSOR_MODE),
		"Game2", static_cast<int>(InputMode::GAME2_MODE)
	);

	lua.new_enum("RenderMode",
		"Normal", static_cast<int>(RenderMode::Normal),
		"Wireframe", static_cast<int>(RenderMode::Wireframe),
		"Normals", static_cast<int>(RenderMode::Normals),
		"Depth", static_cast<int>(RenderMode::Depth),
		"Albedo", static_cast<int>(RenderMode::Albedo),
		"Metallic", static_cast<int>(RenderMode::Metallic),
		"Roughness", static_cast<int>(RenderMode::Roughness),
		"AO", static_cast<int>(RenderMode::AO),
		"Lighting", static_cast<int>(RenderMode::Lighting),
		"Cascades", static_cast<int>(RenderMode::Cascades),
		"TileHeatmap", static_cast<int>(RenderMode::TileHeatmap),
		"UV", static_cast<int>(RenderMode::UV),
		"Tangents", static_cast<int>(RenderMode::Tangents),
		"Reserve1", static_cast<int>(RenderMode::Reserve1),
		"Reserve2", static_cast<int>(RenderMode::Reserve2),
		"Reserve3", static_cast<int>(RenderMode::Reserve3),
		"Reserve4", static_cast<int>(RenderMode::Reserve4),
		"Reserve5", static_cast<int>(RenderMode::Reserve5),
		"Reserve6", static_cast<int>(RenderMode::Reserve6),
		"Reserve7", static_cast<int>(RenderMode::Reserve7),
		"Reserve8", static_cast<int>(RenderMode::Reserve8)
	);

	lua.new_enum("MaterialFiltering",
		"Default", static_cast<int>(MaterialFiltering::Default),
		"Nearest", static_cast<int>(MaterialFiltering::Nearest)
	);

    // ============================================================
    // Регистрация ObjectRef
    // ============================================================
    auto ut = lua.new_usertype<ObjectRef>("ObjectRef");

    // --- Методы ---
    ut["destroy"] = [](ObjectRef& ref) { ref.app->removeGameObject(ref.id); };

    ut["setParent"] = [](ObjectRef& ref, ObjectRef& parent) {
        ref.app->setParent(ref.id, parent.id);
    };

    ut["getChildren"] = [](ObjectRef& ref) -> sol::table {
        GameObject* go = ref.get();
        if (!go) return ref.app->m_scriptEngine->lua().create_table();
        sol::state& L = ref.app->m_scriptEngine->lua();
        sol::table result = L.create_table();
        int idx = 1;
        for (uint32_t cid : go->childrenIds)
            result[idx++] = ObjectRef{cid, ref.app};
        return result;
    };

    // --- Общие свойства ---
    ut["id"] = sol::readonly_property([](ObjectRef& ref) -> uint32_t { return ref.id; });
    ut["type"] = sol::readonly_property([](ObjectRef& ref) -> std::string {
        GameObject* go = ref.get(); return go ? go->getTypeName() : "unknown";
    });

    ut["name"] = sol::property(
        [](ObjectRef& ref) -> std::string { GameObject* go = ref.get(); return go ? go->name : ""; },
        [](ObjectRef& ref, const std::string& val) { GameObject* go = ref.get(); if (go) go->name = val; }
    );

    ut["enabled"] = sol::property(
        [](ObjectRef& ref) -> bool { GameObject* go = ref.get(); return go ? go->enabled : false; },
        [](ObjectRef& ref, bool val) { GameObject* go = ref.get(); if (go) go->enabled = val; }
    );

	ut["position"] = sol::property(
		[](ObjectRef& ref) -> LuaVec3 {
			GameObject* go = ref.get();
			if (!go) return LuaVec3(0);
			return LuaVec3(go->attachToParent ? go->localPosition : go->position);
		},
		[this](ObjectRef& ref, const LuaVec3& val) {
			GameObject* go = ref.get(); if (!go) return;
			if (go->attachToParent) {
				go->localPosition = val.toGlm();
				go->syncLocalQuatFromEuler();
				updateWorldTransform(go->id); // Мгновенно обновляем мир для рендера/физики
			} else {
				go->position = val.toGlm();
				go->syncQuatFromEuler();
				// Мировые позиции компонентов обязаны обновиться здесь же:
				// иначе они устаревают до фазы 1 следующего кадра, и звук,
				// свет и физика на этом кадре считаются от старой точки
				go->updateComponentTransforms();

				// Двигаем ВСЕ тела объекта и каждое — в точку СВОЕГО компонента.
				// Раньше бралось первое и ставилось в go->position: локальный
				// сдвиг тела стирался любой записью позиции объекта.
				for (auto& comp : go->components) {
					if (!comp.enabled) continue;   // выключенный компонент не трогаем
					auto* body = std::get_if<PhysicsBodyComponent>(&comp.data);
					if (!body) continue;
					body->moved = true;
					if (body->physicsBody != INVALID_HANDLE && ref.app->m_physics) {
						ref.app->m_physics->setPositionDirect(body->physicsBody, comp.worldPosition);
						ref.app->m_physics->setRotation(body->physicsBody, comp.worldRotationQuat);
					}
				}
			}
		}
	);

	ut["rotation"] = sol::property(
		[](ObjectRef& ref) -> LuaVec3 {
			GameObject* go = ref.get();
			if (!go) return LuaVec3(0);
			return LuaVec3(go->attachToParent ? go->localRotation : go->rotation);
		},
		[this](ObjectRef& ref, const LuaVec3& val) {
			GameObject* go = ref.get(); if (!go) return;
			if (go->attachToParent) {
				go->localRotation = val.toGlm();
				go->syncLocalQuatFromEuler();
				updateWorldTransform(go->id);
			} else {
				go->rotation = val.toGlm();
				go->syncQuatFromEuler();
				if (auto* body = getComponent<PhysicsBodyComponent>(*go)) {
					body->moved = true;
				}
			}
		}
	);

	// Мировые координаты объекта — только чтение. В отличие от position они не
	// переключаются на локальные при attachToParent
	ut["worldPosition"] = sol::property([](ObjectRef& ref) -> LuaVec3 {
		GameObject* go = ref.get();
		return go ? LuaVec3(go->position) : LuaVec3(0);
	});

	ut["worldRotation"] = sol::property([](ObjectRef& ref) -> LuaVec3 {
		GameObject* go = ref.get();
		return go ? LuaVec3(go->rotation) : LuaVec3(0);
	});

	// Явные локальные координаты: в отличие от position/rotation они всегда
	// означают смещение относительно родителя, независимо от attachToParent
	ut["localPosition"] = sol::property(
		[](ObjectRef& ref) -> LuaVec3 {
			GameObject* go = ref.get();
			return go ? LuaVec3(go->localPosition) : LuaVec3(0);
		},
		[this](ObjectRef& ref, const LuaVec3& val) {
			GameObject* go = ref.get(); if (!go) return;
			go->localPosition = val.toGlm();
			updateWorldTransform(go->id);
		}
	);

	ut["localRotation"] = sol::property(
		[](ObjectRef& ref) -> LuaVec3 {
			GameObject* go = ref.get();
			return go ? LuaVec3(go->localRotation) : LuaVec3(0);
		},
		[this](ObjectRef& ref, const LuaVec3& val) {
			GameObject* go = ref.get(); if (!go) return;
			go->localRotation = val.toGlm();
			go->syncLocalQuatFromEuler();
			updateWorldTransform(go->id);
		}
	);

	ut["attachToParent"] = sol::property(
		[](ObjectRef& ref) -> bool {
			GameObject* go = ref.get();
			return go ? go->attachToParent : false;
		},
		[this](ObjectRef& ref, bool val) {
			GameObject* go = ref.get(); if (!go) return;
			if (go->attachToParent == val) return; // Уже в этом состоянии

			if (val) {
				// Включаем: текущие мировые становятся локальными, чтобы объект не прыгнул
				go->localPosition = go->position;
				go->localRotation = go->rotation;
				go->syncLocalQuatFromEuler();
			} else {
				// Выключаем: локальные становятся мировыми
				go->position = go->localPosition;
				go->rotation = go->localRotation;
				go->syncQuatFromEuler();
			}
			go->attachToParent = val;

			// Обновляем кэш детей у родителей
			if (go->parentId != 0 && go->autoUpdateChildren) {
				// (Логика уже есть в setParent, но здесь мы просто меняем флаг)
			}
			updateWorldTransform(go->id);
		}
	);

    ut["scale"] = sol::property(
        [](ObjectRef& ref) -> LuaVec3 { GameObject* go = ref.get(); return go ? LuaVec3(go->scale) : LuaVec3(1); },
        [](ObjectRef& ref, const LuaVec3& val) { GameObject* go = ref.get(); if (go) go->scale = val.toGlm(); }
    );

	ut["parent"] = sol::property(
		[](ObjectRef& ref) -> sol::object {
			GameObject* go = ref.get();
			if (!go || go->parentId == 0) return sol::object(); // nil
			return sol::make_object(ref.app->m_scriptEngine->lua(), ObjectRef{go->parentId, ref.app});
		},
		[this](ObjectRef& ref, sol::object parentArg) {
			GameObject* go = ref.get();
			if (!go) return;
			GameObject* parent = resolveObjectArg(parentArg); // Работает со строкой, ID или ObjectRef!
			uint32_t newParentId = parent ? parent->id : 0;
			ref.app->setParent(go->id, newParentId);
		}
	);

	// ============================================================
	// ПЕРСОНАЛЬНЫЕ КОЛЛБЕКИ СОБЫТИЙ
	// ============================================================
	ut["onCollisionEnter"] = sol::property(
		[](ObjectRef& ref) -> sol::table {
			sol::state& lua = ref.app->m_scriptEngine->lua();
			sol::table result = lua.create_table();
			auto it = ref.app->m_objectCallbacks.find(ref.id);
			if (it != ref.app->m_objectCallbacks.end()) {
				int idx = 1;
				for (const auto& func : it->second.onCollisionEnter) {
					if (func.valid()) result[idx++] = func;
				}
			}
			return result; // Возвращаем таблицу со всеми функциями
		},
		[](ObjectRef& ref, sol::protected_function func) {
			if (func.valid()) ref.app->m_objectCallbacks[ref.id].onCollisionEnter.push_back(func);
		}
	);

	ut["onCollisionExit"] = sol::property(
		[](ObjectRef& ref) -> sol::table {
			sol::state& lua = ref.app->m_scriptEngine->lua();
			sol::table result = lua.create_table();
			auto it = ref.app->m_objectCallbacks.find(ref.id);
			if (it != ref.app->m_objectCallbacks.end()) {
				int idx = 1;
				for (const auto& func : it->second.onCollisionExit) {
					if (func.valid()) result[idx++] = func;
				}
			}
			return result;
		},
		[](ObjectRef& ref, sol::protected_function func) {
			if (func.valid()) ref.app->m_objectCallbacks[ref.id].onCollisionExit.push_back(func);
		}
	);

	ut["onTriggerEnter"] = sol::property(
		[](ObjectRef& ref) -> sol::table {
			sol::state& lua = ref.app->m_scriptEngine->lua();
			sol::table result = lua.create_table();
			auto it = ref.app->m_objectCallbacks.find(ref.id);
			if (it != ref.app->m_objectCallbacks.end()) {
				int idx = 1;
				for (const auto& func : it->second.onTriggerEnter) {
					if (func.valid()) result[idx++] = func;
				}
			}
			return result;
		},
		[](ObjectRef& ref, sol::protected_function func) {
			if (func.valid()) ref.app->m_objectCallbacks[ref.id].onTriggerEnter.push_back(func);
		}
	);

	ut["onTriggerExit"] = sol::property(
		[](ObjectRef& ref) -> sol::table {
			sol::state& lua = ref.app->m_scriptEngine->lua();
			sol::table result = lua.create_table();
			auto it = ref.app->m_objectCallbacks.find(ref.id);
			if (it != ref.app->m_objectCallbacks.end()) {
				int idx = 1;
				for (const auto& func : it->second.onTriggerExit) {
					if (func.valid()) result[idx++] = func;
				}
			}
			return result;
		},
		[](ObjectRef& ref, sol::protected_function func) {
			if (func.valid()) ref.app->m_objectCallbacks[ref.id].onTriggerExit.push_back(func);
		}
	);

    // ============================================================
    // Body-специфичные
    // ============================================================
	ut["mesh"] = sol::property(
		[this](ObjectRef& ref) -> sol::object {
			GameObject* go = ref.get(); if (!go) return sol::object();
			if (auto* body = getComponent<MeshComponent>(*go)) {
				std::string name = getMeshNameByHandle(body->renderable);
				if (!name.empty()) return sol::make_object(m_scriptEngine->lua(), name);
			}
			return sol::object();
		},
		[this](ObjectRef& ref, const std::string& val) {
			GameObject* go = ref.get(); if (!go) return;
			if (auto* body = getComponent<MeshComponent>(*go)) {
				body->renderable = getMeshHandleByName(val);
				// Веса форм принадлежали прошлому мешу: у нового формы другие
				// (или их нет вовсе), и оставлять старые числа значит
				// деформировать модель непонятно чем
				body->shapeKeyWeights.clear();
				// === ОБНОВЛЕНИЕ localBounds ===
				if (const MeshAsset* m = getMeshByName(val)) {
					body->modelOffset = m->offset;
					body->localBounds = m->bounds;
				}
			}
		}
	);

    ut["material"] = sol::property(
        [this](ObjectRef& ref) -> sol::object {
            GameObject* go = ref.get(); if (!go) return sol::object();
            if (auto* body = getComponent<MeshComponent>(*go)) {
                std::string name = getMaterialNameByHandle(body->materialHandle);
                if (!name.empty()) return sol::make_object(m_scriptEngine->lua(), name);
            }
            return sol::object();
        },
        [this](ObjectRef& ref, const std::string& val) {
            GameObject* go = ref.get(); if (!go) return;
            if (auto* body = getComponent<MeshComponent>(*go))
                body->materialHandle = getMaterialHandleByName(val);
        }
    );

    ut["mass"] = sol::property(
        [](ObjectRef& ref) -> sol::object {
            GameObject* go = ref.get(); if (!go) return sol::object();
            if (auto* body = getComponent<PhysicsBodyComponent>(*go))
                return sol::make_object(ref.app->m_scriptEngine->lua(), body->mass);
            return sol::object();
        },
        [](ObjectRef& ref, float val) {
            GameObject* go = ref.get(); if (!go) return;
            if (auto* body = getComponent<PhysicsBodyComponent>(*go)) body->mass = val;
        }
    );

	ut["bodyFlags"] = sol::property(
        [](ObjectRef& ref) -> sol::object {
            GameObject* go = ref.get(); if (!go) return sol::object();
            if (auto* body = getComponent<MeshComponent>(*go))
                return sol::make_object(ref.app->m_scriptEngine->lua(), body->flags);
            return sol::object();
        },
        [](ObjectRef& ref, int val) {
            GameObject* go = ref.get(); if (!go) return;
            if (auto* body = getComponent<MeshComponent>(*go)) body->flags = static_cast<uint32_t>(val);
        }
    );

    ut["physicsEnabled"] = sol::property(
        [](ObjectRef& ref) -> sol::object {
            GameObject* go = ref.get(); if (!go) return sol::object();
            if (auto* body = getComponent<PhysicsBodyComponent>(*go))
                return sol::make_object(ref.app->m_scriptEngine->lua(), body->physicsEnabled);
            return sol::object();
        },
        [](ObjectRef& ref, bool val) {
            GameObject* go = ref.get(); if (!go) return;
            if (auto* body = getComponent<PhysicsBodyComponent>(*go)) {
                body->physicsEnabled = val;
                if (body->physicsBody != INVALID_HANDLE && ref.app->m_physics)
                    ref.app->m_physics->setBodyActiveInWorld(body->physicsBody, val);
            }
        }
    );

	ut["overrideCollisionEnabled"] = sol::property(
        [](ObjectRef& ref) -> sol::object {
            GameObject* go = ref.get(); if (!go) return sol::object();
            if (auto* body = getComponent<PhysicsBodyComponent>(*go))
                return sol::make_object(ref.app->m_scriptEngine->lua(), body->collisionEnabled);
            return sol::object();
        },
        [](ObjectRef& ref, bool val) {
            GameObject* go = ref.get(); if (!go) return;
            if (auto* body = getComponent<PhysicsBodyComponent>(*go)) {
                body->collisionEnabled = val;
            }
        }
    );

    ut["castShadow"] = sol::property(
        [](ObjectRef& ref) -> sol::object {
            GameObject* go = ref.get(); if (!go) return sol::object();
            if (auto* body = getComponent<MeshComponent>(*go))
                return sol::make_object(ref.app->m_scriptEngine->lua(), body->castShadow());
            if (auto* light = getComponent<LightComponent>(*go))
				return sol::make_object(ref.app->m_scriptEngine->lua(), light->castShadow);
			return sol::object();
        },
        [](ObjectRef& ref, bool val) {
            GameObject* go = ref.get(); if (!go) return;
            if (auto* body = getComponent<MeshComponent>(*go)) body->setCastShadow(val);
			else if (auto* light = getComponent<LightComponent>(*go)) light->castShadow = val;
        }
    );

    ut["paintColor"] = sol::property(
        [](ObjectRef& ref) -> sol::object {
            GameObject* go = ref.get(); if (!go) return sol::object();
            if (auto* body = getComponent<MeshComponent>(*go)) {
                return sol::make_object(ref.app->m_scriptEngine->lua(), LuaVec4(body->paintColor));
            }
            return sol::object();
        },
        [](ObjectRef& ref, const LuaVec4& val) {
            GameObject* go = ref.get(); if (!go) return;
            if (auto* body = getComponent<MeshComponent>(*go)) {
                body->paintColor = val.toGlm();
            }
        }
    );

    // ============================================================
    // Light-специфичные
    // ============================================================
    ut["intensity"] = sol::property(
        [](ObjectRef& ref) -> sol::object {
            GameObject* go = ref.get(); if (!go) return sol::object();
            if (auto* light = getComponent<LightComponent>(*go))
                return sol::make_object(ref.app->m_scriptEngine->lua(), light->intensity);
            return sol::object();
        },
        [](ObjectRef& ref, float val) {
            GameObject* go = ref.get(); if (!go) return;
            if (auto* light = getComponent<LightComponent>(*go)) light->intensity = val;
        }
    );

    ut["radius"] = sol::property(
        [](ObjectRef& ref) -> sol::object {
            GameObject* go = ref.get(); if (!go) return sol::object();
            if (auto* light = getComponent<LightComponent>(*go))
                return sol::make_object(ref.app->m_scriptEngine->lua(), light->radius);
            return sol::object();
        },
        [](ObjectRef& ref, float val) {
            GameObject* go = ref.get(); if (!go) return;
            // Нулевой и отрицательный радиус дают вырожденную проекцию тени
            // (glm::perspective с farPlane = 0) и бессмысленный AABB в
            // кластеризации. В редакторе тот же параметр зажат в [1, 100].
            if (auto* light = getComponent<LightComponent>(*go))
                light->radius = std::max(val, 0.01f);
        }
    );

	ut["lightType"] = sol::property(
		[](ObjectRef& ref) -> int {
			GameObject* go = ref.get(); if (!go) return 0;
			if (auto* light = getComponent<LightComponent>(*go)) return static_cast<int>(light->type);
			return 0;
		},
		[this](ObjectRef& ref, int val) {
			GameObject* go = ref.get(); if (!go) return;
			if (val < 0 || val > static_cast<int>(LightType::Spot)) {
				m_scriptEngine->log("lightType: допустимы 0 (Point), 1 (Directional), 2 (Spot)",
				                    ConsoleLogEntry::Error);
				return;
			}
			if (auto* light = getComponent<LightComponent>(*go)) light->type = static_cast<LightType>(val);
		}
	);

	// Углы конуса и цвет: раньше плоскими свойствами их выставить было нельзя,
	// приходилось лезть через obj:getComponent("light")
	ut["innerConeAngle"] = sol::property(
		[](ObjectRef& ref) -> float {
			GameObject* go = ref.get(); if (!go) return 0.0f;
			if (auto* light = getComponent<LightComponent>(*go)) return light->innerConeAngle;
			return 0.0f;
		},
		[](ObjectRef& ref, float val) {
			GameObject* go = ref.get(); if (!go) return;
			if (auto* light = getComponent<LightComponent>(*go))
				light->innerConeAngle = glm::clamp(val, 0.0f, 89.9f);
		}
	);
	ut["outerConeAngle"] = sol::property(
		[](ObjectRef& ref) -> float {
			GameObject* go = ref.get(); if (!go) return 0.0f;
			if (auto* light = getComponent<LightComponent>(*go)) return light->outerConeAngle;
			return 0.0f;
		},
		[](ObjectRef& ref, float val) {
			GameObject* go = ref.get(); if (!go) return;
			if (auto* light = getComponent<LightComponent>(*go))
				light->outerConeAngle = glm::clamp(val, 0.0f, 89.9f);
		}
	);
	ut["shadowFarPlane"] = sol::property(
		[](ObjectRef& ref) -> float {
			GameObject* go = ref.get(); if (!go) return 0.0f;
			if (auto* light = getComponent<LightComponent>(*go)) return light->shadowFarPlane;
			return 0.0f;
		},
		[](ObjectRef& ref, float val) {
			GameObject* go = ref.get(); if (!go) return;
			// 0 означает «авто» (radius * 1.1), отрицательных быть не должно
			if (auto* light = getComponent<LightComponent>(*go))
				light->shadowFarPlane = std::max(val, 0.0f);
		}
	);

    // ============================================================
    // CustomComponent Fallback (динамические параметры)
    // ============================================================

    // ============================================================
    // Sound-специфичные
    // ============================================================
    ut["soundName"] = sol::property(
        [](ObjectRef& ref) -> sol::object {
            GameObject* go = ref.get(); if (!go) return sol::object();
            if (auto* sound = getComponent<SoundComponent>(*go))
                return sol::make_object(ref.app->m_scriptEngine->lua(), sound->name);
            return sol::object();
        },
        [](ObjectRef& ref, const std::string& val) {
            GameObject* go = ref.get(); if (!go) return;
            if (auto* sound = getComponent<SoundComponent>(*go)) {
                sound->name = val;
                sound->isPlaying = false;
            }
        }
    );

    // Свойства
    ut["minDistance"] = sol::property(
        [](ObjectRef& ref) -> float {
            auto* s = ref.get() ? getComponent<SoundComponent>(*ref.get()) : nullptr;
            return s ? s->minDistance : 0.0f;
        },
        [](ObjectRef& ref, float val) {
            auto* s = ref.get() ? getComponent<SoundComponent>(*ref.get()) : nullptr;
            if (s) {
                s->minDistance = val;
                if (s->isPlaying && s->instanceId != 0 && ref.app->m_audio) ref.app->m_audio->setMinDistance(s->instanceId, val);
            }
        }
    );
    ut["maxDistance"] = sol::property(
        [](ObjectRef& ref) -> float {
            auto* s = ref.get() ? getComponent<SoundComponent>(*ref.get()) : nullptr;
            return s ? s->maxDistance : 0.0f;
        },
        [](ObjectRef& ref, float val) {
            auto* s = ref.get() ? getComponent<SoundComponent>(*ref.get()) : nullptr;
            if (s) {
                s->maxDistance = val;
                if (s->isPlaying && s->instanceId != 0 && ref.app->m_audio) ref.app->m_audio->setMaxDistance(s->instanceId, val);
            }
        }
    );
    ut["rolloff"] = sol::property(
        [](ObjectRef& ref) -> float {
            auto* s = ref.get() ? getComponent<SoundComponent>(*ref.get()) : nullptr;
            return s ? s->rolloff : 0.0f;
        },
        [](ObjectRef& ref, float val) {
            auto* s = ref.get() ? getComponent<SoundComponent>(*ref.get()) : nullptr;
            if (s) {
                s->rolloff = val;
                if (s->isPlaying && s->instanceId != 0 && ref.app->m_audio) ref.app->m_audio->setRolloff(s->instanceId, val);
            }

        }
    );

    ut["volume"] = sol::property(
        [](ObjectRef& ref) -> sol::object {
            GameObject* go = ref.get(); if (!go) return sol::object();
            if (auto* sound = getComponent<SoundComponent>(*go))
                return sol::make_object(ref.app->m_scriptEngine->lua(), sound->volume);
            return sol::object();
        },
        [](ObjectRef& ref, float val) {
            GameObject* go = ref.get(); if (!go) return;
            if (auto* sound = getComponent<SoundComponent>(*go)) {
                sound->volume = val;
                if (sound->isPlaying && sound->instanceId != 0 && ref.app->m_audio)
                    ref.app->m_audio->setVolume(sound->instanceId, val);
            }
        }
    );

	ut["particleDirection"] = sol::property(
		[](ObjectRef& ref) -> sol::object {
			GameObject* go = ref.get(); if (!go) return sol::object();
			if (auto* emitter = getComponent<ParticleEmitterComponent>(*go))
				return sol::make_object(ref.app->m_scriptEngine->lua(), LuaVec3(emitter->particleDirection));
			if (auto* sp = getComponent<SingleParticleComponent>(*go))
				return sol::make_object(ref.app->m_scriptEngine->lua(), LuaVec3(sp->particleDirection));
			return sol::object();
		},
		[](ObjectRef& ref, const LuaVec3& val) {
			GameObject* go = ref.get(); if (!go) return;
			if (auto* emitter = getComponent<ParticleEmitterComponent>(*go)) {
				emitter->particleDirection = val.toGlm();
			} else if (auto* sp = getComponent<SingleParticleComponent>(*go)) {
				sp->particleDirection = val.toGlm();
			}
		}
	);

	ut["orientation"] = sol::property(
		[](ObjectRef& ref) -> int {
			GameObject* go = ref.get(); if (!go) return 0;
			if (auto* emitter = getComponent<ParticleEmitterComponent>(*go)) return static_cast<int>(emitter->orientation);
			if (auto* sp = getComponent<SingleParticleComponent>(*go)) return static_cast<int>(sp->orientation);
			return 0;
		},
		[](ObjectRef& ref, int val) {
			GameObject* go = ref.get(); if (!go) return;
			auto orient = static_cast<ParticleOrientation>(val);
			if (auto* emitter = getComponent<ParticleEmitterComponent>(*go)) emitter->orientation = orient;
			else if (auto* sp = getComponent<SingleParticleComponent>(*go)) sp->orientation = orient;
		}
	);

	ut["lightInfluence"] = sol::property(
		[](ObjectRef& ref) -> sol::object {
			GameObject* go = ref.get(); if (!go) return sol::object();
			if (auto* emitter = getComponent<ParticleEmitterComponent>(*go))
				return sol::make_object(ref.app->m_scriptEngine->lua(), emitter->lightInfluence);
			if (auto* sp = getComponent<SingleParticleComponent>(*go))
				return sol::make_object(ref.app->m_scriptEngine->lua(), sp->lightInfluence);
			return sol::object();
		},
		[](ObjectRef& ref, float val) {
			GameObject* go = ref.get(); if (!go) return;
			if (auto* emitter = getComponent<ParticleEmitterComponent>(*go)) emitter->lightInfluence = glm::clamp(val, 0.0f, 1.0f);
			else if (auto* sp = getComponent<SingleParticleComponent>(*go)) sp->lightInfluence = glm::clamp(val, 0.0f, 1.0f);
		}
	);

	ut["soft"] = sol::property(
		[](ObjectRef& ref) -> sol::object {
			GameObject* go = ref.get(); if (!go) return sol::object();
			if (auto* emitter = getComponent<ParticleEmitterComponent>(*go))
				return sol::make_object(ref.app->m_scriptEngine->lua(), emitter->soft);
			if (auto* sp = getComponent<SingleParticleComponent>(*go))
				return sol::make_object(ref.app->m_scriptEngine->lua(), sp->soft);
			return sol::object();
		},
		[](ObjectRef& ref, bool val) {
			GameObject* go = ref.get(); if (!go) return;
			if (auto* emitter = getComponent<ParticleEmitterComponent>(*go)) emitter->soft = val;
			else if (auto* sp = getComponent<SingleParticleComponent>(*go)) sp->soft = val;
		}
	);

    ut["is3DSound"] = sol::property(
        [](ObjectRef& ref) -> sol::object {
            GameObject* go = ref.get(); if (!go) return sol::object();
            if (auto* sound = getComponent<SoundComponent>(*go))
                return sol::make_object(ref.app->m_scriptEngine->lua(), sound->type == 1);
            return sol::object();
        },
        [](ObjectRef& ref, bool val) {
            GameObject* go = ref.get(); if (!go) return;
            if (auto* sound = getComponent<SoundComponent>(*go)) {
                sound->type = val ? 1 : 0;
                sound->isPlaying = false;
            }
        }
    );

    ut["loop"] = sol::property(
        [](ObjectRef& ref) -> sol::object {
            GameObject* go = ref.get(); if (!go) return sol::object();
            if (auto* sound = getComponent<SoundComponent>(*go))
                return sol::make_object(ref.app->m_scriptEngine->lua(), sound->loop);
            return sol::object();
        },
        [](ObjectRef& ref, bool val) {
            GameObject* go = ref.get(); if (!go) return;
            if (auto* sound = getComponent<SoundComponent>(*go)) sound->loop = val;
        }
    );

    // Флаг есть и у звука, и у анимации. Раньше свойство знало только про звук,
    // поэтому playClip выставлял anim->isPlaying, а прочитать его из Lua было нечем.
    ut["isPlaying"] = sol::property(
        [](ObjectRef& ref) -> sol::object {
            GameObject* go = ref.get(); if (!go) return sol::object();
            if (auto* sound = getComponent<SoundComponent>(*go))
                return sol::make_object(ref.app->m_scriptEngine->lua(), sound->isPlaying);
            if (auto* anim = getComponent<AnimationComponent>(*go))
                return sol::make_object(ref.app->m_scriptEngine->lua(), anim->isPlaying);
            return sol::object();
        },
        [](ObjectRef& ref, bool val) {
            GameObject* go = ref.get(); if (!go) return;
            if (auto* sound = getComponent<SoundComponent>(*go)) sound->isPlaying = val;
            else if (auto* anim = getComponent<AnimationComponent>(*go)) anim->isPlaying = val;
        }
    );

    // ============================================================
    // Sound Control Method (Только Play, Stop - объединен ниже)
    // ============================================================
    ut["play"] = [this](ObjectRef& ref) {
        GameObject* go = ref.get(); if (!go) return;
        auto* sound = getComponent<SoundComponent>(*go);
        if (!sound || sound->name.empty() || !m_audio) return;
        if (sound->instanceId != 0) m_audio->stop(sound->instanceId);

        if (sound->type == 1) {
            sound->instanceId = m_audio->play(sound->name, sound->volume, sound->pitch, sound->loop, &go->position, sound->minDistance, sound->maxDistance, sound->rolloff);
            sound->prevPosition = go->position;
        } else {
            sound->instanceId = m_audio->play(sound->name, sound->volume, sound->pitch, sound->loop, nullptr, 0.0f, 0.0f, 0.0f);
        }
        sound->isPlaying = true;
    };

    // ============================================================
    // Script-специфичные свойства
    // ============================================================
    ut["scriptText"] = sol::property(
        [](ObjectRef& ref) -> std::string {
            GameObject* go = ref.get(); if (!go) return "";
            if (auto* script = getComponent<ScriptComponent>(*go))
                return script->scriptText;
            return "";
        },
        [](ObjectRef& ref, const std::string& val) {
            GameObject* go = ref.get(); if (!go) return;
            if (auto* script = getComponent<ScriptComponent>(*go))
                script->scriptText = val;
        }
    );

    ut["startOnLoad"] = sol::property(
        [](ObjectRef& ref) -> bool {
            GameObject* go = ref.get(); if (!go) return false;
            if (auto* script = getComponent<ScriptComponent>(*go))
                return script->startOnLoad;
            return false;
        },
        [](ObjectRef& ref, bool val) {
            GameObject* go = ref.get(); if (!go) return;
            if (auto* script = getComponent<ScriptComponent>(*go))
                script->startOnLoad = val;
        }
    );

    // Запуск ScriptComponent. Раньше единственной точкой старта была
    // loadScene по startOnLoad: компонент, добавленный через addComponent
    // или в редакторе, оставался мёртвым до следующей загрузки сцены.
    ut["runScript"] = [this](ObjectRef& ref) -> bool {
        GameObject* go = ref.get(); if (!go) return false;
        auto* script = getComponent<ScriptComponent>(*go);
        if (!script) {
            m_scriptEngine->log("runScript: у объекта нет компонента script",
                                ConsoleLogEntry::Error);
            return false;
        }
        if (script->scriptText.empty()) {
            m_scriptEngine->log("runScript: текст скрипта пуст", ConsoleLogEntry::Error);
            return false;
        }
        // Перезапуск: движок держит по одному запущенному скрипту на объект
        if (script->isRunning) m_scriptEngine->unregisterScriptObject(go->id);
        m_scriptEngine->registerScriptObject(go->id, script->scriptText);
        m_scriptEngine->startScriptObject(go->id);
        script->isRunning = true;
        return true;
    };

    ut["isRunning"] = sol::readonly_property([](ObjectRef& ref) -> bool {
        GameObject* go = ref.get(); if (!go) return false;
        if (auto* script = getComponent<ScriptComponent>(*go))
            return script->isRunning;
        return false;
    });

    ut["call"] = [this](ObjectRef& ref, const std::string& funcName, sol::optional<sol::object> param) -> sol::object {
        GameObject* go = ref.get(); if (!go) return sol::nil;
        auto* script = getComponent<ScriptComponent>(*go);
        if (!script || !m_scriptEngine || !script->isRunning) return sol::nil;

        return m_scriptEngine->callScriptObjectFunc(go->id, funcName, param.value_or(sol::nil));
    };

    // Обработчик присваивания неизвестных свойств зарегистрирован ниже, в
    // конце bindObjectAPI: sol2 оставляет последнюю регистрацию, поэтому
    // копия, стоявшая здесь, всё равно затиралась — и при этом расходилась
    // с рабочей по поведению (не ругалась на объект без CustomComponent).

    // ============================================================
    // Animation-специфичные свойства
    // ============================================================
    ut["isPlayingAnim"] = sol::readonly_property([](ObjectRef& ref) -> bool {
        GameObject* go = ref.get(); if (!go) return false;
        if (auto* anim = getComponent<AnimationComponent>(*go)) return anim->isPlaying;
        return false;
    });

    // Возвращает признак успеха: опечатка в имени клипа раньше молча
    // не делала ничего, и это находилось уже по «анимация не играет»
    ut["playClip"] = [](ObjectRef& ref, const std::string& clipName) -> bool {
        GameObject* go = ref.get(); if (!go) return false;
        auto* anim = getComponent<AnimationComponent>(*go); if (!anim) return false;
        if (anim->clips.find(clipName) == anim->clips.end()) return false;

        anim->currentClipName = clipName;
        anim->currentTime = 0.0f;
        anim->isPlaying = true;
        anim->isPaused = false;
        return true;
    };

    ut["stop"] = [this](ObjectRef& ref) {
        GameObject* go = ref.get(); if (!go) return;

        // Логика Sound
        if (auto* sound = getComponent<SoundComponent>(*go)) {
            if (m_audio && sound->instanceId != 0) m_audio->stop(sound->instanceId);
            sound->isPlaying = false; sound->instanceId = 0; return;
        }
        // Логика Script
        if (auto* script = getComponent<ScriptComponent>(*go)) {
            if (script->isRunning && m_scriptEngine) { m_scriptEngine->unregisterScriptObject(go->id); script->isRunning = false; } return;
        }
        // Логика Animation
        if (auto* anim = getComponent<AnimationComponent>(*go)) {
            anim->isPlaying = false; anim->isPaused = false; anim->currentTime = 0.0f; anim->currentClipName = ""; return;
        }
    };

    ut["pause"] = [](ObjectRef& ref) {
        GameObject* go = ref.get(); if (!go) return;
        if (auto* anim = getComponent<AnimationComponent>(*go)) anim->isPaused = true;
    };

    ut["unpause"] = [](ObjectRef& ref) {
        GameObject* go = ref.get(); if (!go) return;
        if (auto* anim = getComponent<AnimationComponent>(*go)) anim->isPaused = false;
    };

    ut["animTime"] = sol::property(
        [](ObjectRef& ref) -> float {
            GameObject* go = ref.get(); if (!go) return 0.0f;
            if (auto* anim = getComponent<AnimationComponent>(*go)) return anim->currentTime;
            return 0.0f;
        },
        [](ObjectRef& ref, float t) {
            GameObject* go = ref.get(); if (!go) return;
            if (auto* anim = getComponent<AnimationComponent>(*go)) anim->currentTime = t;
        }
    );

    ut["attachObject"] = [](ObjectRef& ref, uint32_t objId, uint32_t trackId, sol::optional<std::string> slotName) {
        GameObject* go = ref.get(); if (!go) return;
        if (auto* anim = getComponent<AnimationComponent>(*go)) {
            anim->bindings.push_back({trackId, objId, slotName.value_or("")});
        }
    };

    ut["detachObject"] = [](ObjectRef& ref, uint32_t objId) {
        GameObject* go = ref.get(); if (!go) return;
        if (auto* anim = getComponent<AnimationComponent>(*go)) {
            anim->bindings.erase(std::remove_if(anim->bindings.begin(), anim->bindings.end(),
                [objId](const AnimBinding& b) { return b.objectId == objId; }), anim->bindings.end());
        }
    };

	ut["clearBindings"] = [](ObjectRef& ref) {
        GameObject* go = ref.get(); if (!go) return;
        if (auto* anim = getComponent<AnimationComponent>(*go)) {
            anim->bindings.clear();
        }
    };

    ut["getClipNames"] = [](ObjectRef& ref) -> sol::table {
        GameObject* go = ref.get();
        if (!go) return ref.app->m_scriptEngine->lua().create_table();
        auto* anim = getComponent<AnimationComponent>(*go);
        if (!anim) return ref.app->m_scriptEngine->lua().create_table();

        sol::table result = ref.app->m_scriptEngine->lua().create_table();
        int idx = 1;
        for (const auto& [name, clip] : anim->clips) result[idx++] = name;
        return result;
    };

    ut["currentClip"] = sol::readonly_property([](ObjectRef& ref) -> std::string {
        GameObject* go = ref.get(); if (!go) return "";
        if (auto* anim = getComponent<AnimationComponent>(*go)) return anim->currentClipName;
        return "";
    });

    ut["removeClip"] = [](ObjectRef& ref, const std::string& clipName) {
        GameObject* go = ref.get(); if (!go) return;
        if (auto* anim = getComponent<AnimationComponent>(*go)) {
            anim->clips.erase(clipName);
            if (anim->currentClipName == clipName) {
                anim->isPlaying = false; anim->currentClipName = "";
            }
        }
    };

    // Путь проверяем, как и во всех остальных Lua-функциях с путями:
    // без этого скрипт мог писать и читать за пределами проекта
    ut["saveAnim"] = [this](ObjectRef& ref, const std::string& path) -> bool {
        GameObject* go = ref.get(); if (!go) return false;
        if (!isPathSafe(path)) {
            m_scriptEngine->log("saveAnim: Access denied to path: " + path, ConsoleLogEntry::Error);
            return false;
        }
        auto* anim = getComponent<AnimationComponent>(*go); if (!anim) return false;
        saveAnimationFile(path, anim);
        return true;
    };

    ut["loadClip"] = [this](ObjectRef& ref, const std::string& path) -> bool {
        GameObject* go = ref.get(); if (!go) return false;
        if (!isPathSafe(path)) {
            m_scriptEngine->log("loadClip: Access denied to path: " + path, ConsoleLogEntry::Error);
            return false;
        }
        auto* anim = getComponent<AnimationComponent>(*go); if (!anim) return false;
        return loadAnimationFile(path, anim);
    };

    ut["createClip"] = [](ObjectRef& ref, const std::string& clipName, float duration, bool loop) {
        GameObject* go = ref.get(); if (!go) return;
        if (auto* anim = getComponent<AnimationComponent>(*go)) {
            if (anim->clips.find(clipName) == anim->clips.end()) {
                AnimClipData clip;
                clip.duration = duration;
                clip.loop = loop;
                clip.attachedFollowsTransform = false; // Important: абсолютные координаты
                anim->clips[clipName] = clip;
            }
        }
    };

    ut["addTrack"] = [](ObjectRef& ref, const std::string& clipName, uint32_t trackId, const std::string& targetName) {
        GameObject* go = ref.get(); if (!go) return;
        if (auto* anim = getComponent<AnimationComponent>(*go)) {
            auto it = anim->clips.find(clipName);
            if (it != anim->clips.end()) {
                for (const auto& t : it->second.tracks) if (t.trackId == trackId) return;
                it->second.tracks.push_back({trackId, targetName, {}});
            }
        }
    };

    ut["addKeyframe"] = [](ObjectRef& ref, const std::string& clipName, uint32_t trackId, float time, const LuaVec3& pos, const LuaVec3& rot, const LuaVec3& scl) {
        GameObject* go = ref.get(); if (!go) return;
        if (auto* anim = getComponent<AnimationComponent>(*go)) {
            auto it = anim->clips.find(clipName);
            if (it != anim->clips.end()) {
                for (auto& track : it->second.tracks) {
                    if (track.trackId == trackId) {
                        AnimKeyframe kf; kf.time = time; kf.position = pos.toGlm(); kf.rotation = rot.toGlm(); kf.scale = scl.toGlm();
                        track.keyframes.push_back(kf);
                        std::sort(track.keyframes.begin(), track.keyframes.end(), [](const AnimKeyframe& a, const AnimKeyframe& b) { return a.time < b.time; });
                        break;
                    }
                }
            }
        }
    };

	// ============================================================
	// Constraint-специфичные свойства
	// ============================================================
	ut["constraintType"] = sol::property(
		[](ObjectRef& ref) -> int {
			GameObject* go = ref.get(); if (!go) return 0;
			if (auto* c = getComponent<ConstraintComponent>(*go)) return static_cast<int>(c->type);
			return 0;
		},
		[this](ObjectRef& ref, int val) {
			GameObject* go = ref.get(); if (!go) return;
			// Допустимы только 1..6: ноль и всё, что вне диапазона, молча
			// давали констрейнт, который switch в applyConstraint не создаёт
			if (val < ConstraintComponent::PointToPoint || val > ConstraintComponent::Cable) {
				m_scriptEngine->log("constraintType: допустимы 1 (PointToPoint), 2 (Hinge), "
				                    "3 (Slider), 4 (Fixed), 5 (Spring), 6 (Cable)",
				                    ConsoleLogEntry::Error);
				return;
			}
			if (auto* c = getComponent<ConstraintComponent>(*go)) c->type = static_cast<ConstraintComponent::Type>(val);
		}
	);

	ut["targetA"] = sol::property(
		[](ObjectRef& ref) -> uint32_t {
			GameObject* go = ref.get();
			if (auto* c = go ? getComponent<ConstraintComponent>(*go) : nullptr) return c->targetA;
			return 0;
		},
		[this](ObjectRef& ref, sol::object val) { // <-- Принимаем sol::object
			GameObject* go = ref.get(); if (!go) return;
			auto* c = getComponent<ConstraintComponent>(*go); if (!c) return;
			GameObject* target = resolveObjectArg(val); // <-- Магия!
			if (target) c->targetA = target->id;
			else m_scriptEngine->log("Constraint targetA: Object not found", ConsoleLogEntry::Warning);
		}
	);

	ut["targetB"] = sol::property(
		[](ObjectRef& ref) -> uint32_t {
			GameObject* go = ref.get();
			if (auto* c = go ? getComponent<ConstraintComponent>(*go) : nullptr) return c->targetB;
			return 0;
		},
		[this](ObjectRef& ref, sol::object val) {
			GameObject* go = ref.get(); if (!go) return;
			auto* c = getComponent<ConstraintComponent>(*go); if (!c) return;
			GameObject* target = resolveObjectArg(val);
			if (target) c->targetB = target->id;
			else m_scriptEngine->log("Constraint targetB: Object not found", ConsoleLogEntry::Warning);
		}
	);

	ut["pivotA"] = sol::property(
		[](ObjectRef& ref) -> LuaVec3 {
			GameObject* go = ref.get();
			if (auto* c = go ? getComponent<ConstraintComponent>(*go) : nullptr) return LuaVec3(c->params.pivotA);
			return LuaVec3(0);
		},
		[](ObjectRef& ref, const LuaVec3& val) {
			GameObject* go = ref.get();
			if (auto* c = go ? getComponent<ConstraintComponent>(*go) : nullptr) c->params.pivotA = val.toGlm();
		}
	);

	ut["pivotB"] = sol::property(
		[](ObjectRef& ref) -> LuaVec3 {
			GameObject* go = ref.get();
			if (auto* c = go ? getComponent<ConstraintComponent>(*go) : nullptr) return LuaVec3(c->params.pivotB);
			return LuaVec3(0);
		},
		[](ObjectRef& ref, const LuaVec3& val) {
			GameObject* go = ref.get();
			if (auto* c = go ? getComponent<ConstraintComponent>(*go) : nullptr) c->params.pivotB = val.toGlm();
		}
	);

	ut["axisA"] = sol::property(
		[](ObjectRef& ref) -> LuaVec3 {
			GameObject* go = ref.get();
			if (auto* c = go ? getComponent<ConstraintComponent>(*go) : nullptr) return LuaVec3(c->params.axisA);
			return LuaVec3(0);
		},
		[](ObjectRef& ref, const LuaVec3& val) {
			GameObject* go = ref.get();
			if (auto* c = go ? getComponent<ConstraintComponent>(*go) : nullptr) c->params.axisA = val.toGlm();
		}
	);

	ut["axisB"] = sol::property(
		[](ObjectRef& ref) -> LuaVec3 {
			GameObject* go = ref.get();
			if (auto* c = go ? getComponent<ConstraintComponent>(*go) : nullptr) return LuaVec3(c->params.axisB);
			return LuaVec3(0);
		},
		[](ObjectRef& ref, const LuaVec3& val) {
			GameObject* go = ref.get();
			if (auto* c = go ? getComponent<ConstraintComponent>(*go) : nullptr) c->params.axisB = val.toGlm();
		}
	);

	ut["rotationA"] = sol::property(
		[](ObjectRef& ref) -> LuaVec3 {
			GameObject* go = ref.get();
			if (auto* c = go ? getComponent<ConstraintComponent>(*go) : nullptr) return LuaVec3(c->params.rotationA);
			return LuaVec3(0);
		},
		[](ObjectRef& ref, const LuaVec3& val) {
			GameObject* go = ref.get();
			if (auto* c = go ? getComponent<ConstraintComponent>(*go) : nullptr) c->params.rotationA = val.toGlm();
		}
	);

	ut["rotationB"] = sol::property(
		[](ObjectRef& ref) -> LuaVec3 {
			GameObject* go = ref.get();
			if (auto* c = go ? getComponent<ConstraintComponent>(*go) : nullptr) return LuaVec3(c->params.rotationB);
			return LuaVec3(0);
		},
		[](ObjectRef& ref, const LuaVec3& val) {
			GameObject* go = ref.get();
			if (auto* c = go ? getComponent<ConstraintComponent>(*go) : nullptr) c->params.rotationB = val.toGlm();
		}
	);

	// --- НОВЫЕ СВОЙСТВА ДЛЯ ПРУЖИН И КАБЕЛЕЙ ---
	ut["stiffness"] = sol::property(
		[](ObjectRef& ref) -> float {
			if (auto* c = ref.get() ? getComponent<ConstraintComponent>(*ref.get()) : nullptr) return c->params.stiffness;
			return 0.0f;
		},
		[](ObjectRef& ref, float val) {
			if (auto* c = ref.get() ? getComponent<ConstraintComponent>(*ref.get()) : nullptr) c->params.stiffness = val;
		}
	);
	ut["damping"] = sol::property(
		[](ObjectRef& ref) -> float {
			if (auto* c = ref.get() ? getComponent<ConstraintComponent>(*ref.get()) : nullptr) return c->params.damping;
			return 0.0f;
		},
		[](ObjectRef& ref, float val) {
			if (auto* c = ref.get() ? getComponent<ConstraintComponent>(*ref.get()) : nullptr) c->params.damping = val;
		}
	);
	ut["maxLength"] = sol::property(
		[](ObjectRef& ref) -> float {
			if (auto* c = ref.get() ? getComponent<ConstraintComponent>(*ref.get()) : nullptr) return c->params.maxLength;
			return 0.0f;
		},
		[](ObjectRef& ref, float val) {
			if (auto* c = ref.get() ? getComponent<ConstraintComponent>(*ref.get()) : nullptr) c->params.maxLength = val;
		}
	);
	ut["decorative"] = sol::property(
		[](ObjectRef& ref) -> bool {
			if (auto* c = ref.get() ? getComponent<ConstraintComponent>(*ref.get()) : nullptr) return c->isDecorative;
			return false;
		},
		[](ObjectRef& ref, bool val) {
			if (auto* c = ref.get() ? getComponent<ConstraintComponent>(*ref.get()) : nullptr) c->isDecorative = val;
		}
	);
	ut["visualize"] = sol::property(
		[](ObjectRef& ref) -> bool {
			if (auto* c = ref.get() ? getComponent<ConstraintComponent>(*ref.get()) : nullptr) return c->visualize;
			return false;
		},
		[](ObjectRef& ref, bool val) {
			if (auto* c = ref.get() ? getComponent<ConstraintComponent>(*ref.get()) : nullptr) c->visualize = val;
		}
	);
	ut["thickness"] = sol::property(
		[](ObjectRef& ref) -> float {
			if (auto* c = ref.get() ? getComponent<ConstraintComponent>(*ref.get()) : nullptr) return c->thickness;
			return 0.0f;
		},
		[](ObjectRef& ref, float val) {
			if (auto* c = ref.get() ? getComponent<ConstraintComponent>(*ref.get()) : nullptr) c->thickness = val;
		}
	);

	ut["applyConstraint"] = [this](ObjectRef& ref) -> bool {
		GameObject* go = ref.get(); if (!go) return false;
		auto* c = getComponent<ConstraintComponent>(*go); if (!c) return false;
		// Вся логика в общем методе: тот же код зовут кнопка в редакторе и
		// восстановление сцены при загрузке
		return applyConstraintComponent(*c);
	};

	ut["removeConstraint"] = [this](ObjectRef& ref) {
		GameObject* go = ref.get(); if (!go) return;
		if (auto* c = getComponent<ConstraintComponent>(*go)) {
			if (c->constraint != INVALID_HANDLE) {
				m_physics->destroyConstraint(c->constraint);
				c->constraint = INVALID_HANDLE;
			}
		}
	};

	// ============================================================
	// Camera-специфичные свойства
	// ============================================================
	ut["fov"] = sol::property(
		[](ObjectRef& ref) -> sol::object {
			GameObject* go = ref.get();
			if (!go) return sol::nil;
			if (auto* cam = getComponent<CameraComponent>(*go))
				return sol::make_object(ref.app->m_scriptEngine->lua(), cam->fov);
			return sol::nil;
		},
		[](ObjectRef& ref, float val) {
			GameObject* go = ref.get();
			if (!go) return;
			if (auto* cam = getComponent<CameraComponent>(*go))
				cam->fov = glm::clamp(val, 10.0f, 120.0f);
		}
	);

	ut["renderTextureSlot"] = sol::property(
		[](ObjectRef& ref) -> sol::object {
			GameObject* go = ref.get();
			if (!go) return sol::nil;
			if (auto* cam = getComponent<CameraComponent>(*go))
				return sol::make_object(ref.app->m_scriptEngine->lua(), cam->renderTextureSlot);
			return sol::nil;
		},
		[](ObjectRef& ref, int slot) {
			GameObject* go = ref.get();
			if (!go) return;
			if (auto* cam = getComponent<CameraComponent>(*go))
				cam->renderTextureSlot = glm::clamp(slot, 0, GraphicsEngineGL::MAX_CAMERA_TARGETS - 1);
		}
	);

	ut["cameraWidth"] = sol::property(
		[](ObjectRef& ref) -> sol::object {
			GameObject* go = ref.get();
			if (!go) return sol::nil;
			if (auto* cam = getComponent<CameraComponent>(*go))
				return sol::make_object(ref.app->m_scriptEngine->lua(), cam->width);
			return sol::nil;
		},
		[](ObjectRef& ref, int w) {
			GameObject* go = ref.get();
			if (!go) return;
			if (auto* cam = getComponent<CameraComponent>(*go))
				cam->width = glm::max(w, 1);
		}
	);

	ut["cameraHeight"] = sol::property(
		[](ObjectRef& ref) -> sol::object {
			GameObject* go = ref.get();
			if (!go) return sol::nil;
			if (auto* cam = getComponent<CameraComponent>(*go))
				return sol::make_object(ref.app->m_scriptEngine->lua(), cam->height);
			return sol::nil;
		},
		[](ObjectRef& ref, int h) {
			GameObject* go = ref.get();
			if (!go) return;
			if (auto* cam = getComponent<CameraComponent>(*go))
				cam->height = glm::max(h, 1);
		}
	);

	ut["cameraBrightness"] = sol::property(
		[](ObjectRef& ref) -> sol::object {
			GameObject* go = ref.get();
			if (!go) return sol::nil;
			if (auto* cam = getComponent<CameraComponent>(*go))
				return sol::make_object(ref.app->m_scriptEngine->lua(), cam->brightness);
			return sol::nil;
		},
		[](ObjectRef& ref, float val) {
			GameObject* go = ref.get();
			if (!go) return;
			if (auto* cam = getComponent<CameraComponent>(*go))
				cam->brightness = glm::clamp(val, 0.0f, 2.0f);
		}
	);

	ut["cameraGamma"] = sol::property(
		[](ObjectRef& ref) -> sol::object {
			GameObject* go = ref.get();
			if (!go) return sol::nil;
			if (auto* cam = getComponent<CameraComponent>(*go))
				return sol::make_object(ref.app->m_scriptEngine->lua(), cam->gamma);
			return sol::nil;
		},
		[](ObjectRef& ref, float val) {
			GameObject* go = ref.get();
			if (!go) return;
			if (auto* cam = getComponent<CameraComponent>(*go))
				cam->gamma = glm::clamp(val, 0.1f, 5.0f);
		}
	);

	ut["cameraSaturation"] = sol::property(
		[](ObjectRef& ref) -> sol::object {
			GameObject* go = ref.get();
			if (!go) return sol::nil;
			if (auto* cam = getComponent<CameraComponent>(*go))
				return sol::make_object(ref.app->m_scriptEngine->lua(), cam->saturation);
			return sol::nil;
		},
		[](ObjectRef& ref, float val) {
			GameObject* go = ref.get();
			if (!go) return;
			if (auto* cam = getComponent<CameraComponent>(*go))
				cam->saturation = glm::clamp(val, 0.0f, 1.0f);
		}
	);

	ut["cameraTonemap"] = sol::property(
		[](ObjectRef& ref) -> int {
			GameObject* go = ref.get(); if (!go) return 0;
			if (auto* cam = getComponent<CameraComponent>(*go)) return cam->tonemapOperator;
			return 0;
		},
		[](ObjectRef& ref, int op) {
			GameObject* go = ref.get(); if (!go) return;
			if (auto* cam = getComponent<CameraComponent>(*go)) cam->tonemapOperator = op;
		}
	);

	ut["cameraChromaticAberration"] = sol::property(
		[](ObjectRef& ref) -> sol::object {
			GameObject* go = ref.get();
			if (!go) return sol::nil;
			if (auto* cam = getComponent<CameraComponent>(*go))
				return sol::make_object(ref.app->m_scriptEngine->lua(), cam->chromaticAberrationStrength);
			return sol::nil;
		},
		[](ObjectRef& ref, float val) {
			GameObject* go = ref.get();
			if (!go) return;
			if (auto* cam = getComponent<CameraComponent>(*go))
				cam->chromaticAberrationStrength = glm::clamp(val, 0.0f, 0.1f);
		}
	);

	ut["cameraNearPlane"] = sol::property(
		[](ObjectRef& ref) -> sol::object {
			GameObject* go = ref.get();
			if (!go) return sol::nil;
			if (auto* cam = getComponent<CameraComponent>(*go))
				return sol::make_object(ref.app->m_scriptEngine->lua(), cam->nearPlane);
			return sol::nil;
		},
		[](ObjectRef& ref, float val) {
			GameObject* go = ref.get();
			if (!go) return;
			if (auto* cam = getComponent<CameraComponent>(*go))
				cam->nearPlane = glm::max(val, 0.01f);
		}
	);

	ut["cameraFarPlane"] = sol::property(
		[](ObjectRef& ref) -> sol::object {
			GameObject* go = ref.get();
			if (!go) return sol::nil;
			if (auto* cam = getComponent<CameraComponent>(*go))
				return sol::make_object(ref.app->m_scriptEngine->lua(), cam->farPlane);
			return sol::nil;
		},
		[](ObjectRef& ref, float val) {
			GameObject* go = ref.get();
			if (!go) return;
			if (auto* cam = getComponent<CameraComponent>(*go))
				cam->farPlane = glm::max(val, 0.1f);
		}
	);

	// === Метод получения текстуры из слота рендера (удобно для материалов) ===
	ut["getRenderTargetHandle"] = [this](ObjectRef& ref) -> TextureHandle {
		GameObject* go = ref.get();
		if (!go) return INVALID_HANDLE;
		if (auto* cam = getComponent<CameraComponent>(*go)) {
			int slot = cam->renderTextureSlot;
			if (slot > 0 && slot < 32) {
				return graphics->getRenderTargetHandle(slot);
			}
		}
		return INVALID_HANDLE;
	};

	// --- Костевые треки анимации ---
	// Трек кости живёт внутри клипа: (clipName, trackId) → кость boneId скелета,
	// на который указывает AnimBinding с этим же trackId.
	// ============================================================
	// AnimGraph — граф состояний анимации (см. other/animgraph.md)
	// ============================================================
	auto animOf = [](ObjectRef& ref) -> AnimationComponent* {
		GameObject* go = ref.get();
		return go ? getComponent<AnimationComponent>(*go) : nullptr;
	};

	// Добавляет состояние; повторный вызов с тем же именем обновляет параметры
	// Возвращает true при успехе — как addBoneTrack/addBoneKeyframe рядом.
	// Опечатка в имени клипа иначе молча давала состояние, которое ничего не
	// проигрывает, и находилось это уже по «персонаж замер» в готовой игре.
	ut["addAnimState"] = [this, animOf](ObjectRef& ref, const std::string& name, const std::string& clipName,
	                              sol::optional<float> speed, sol::optional<bool> loop) -> bool {
		auto* anim = animOf(ref); if (!anim) return false;

		if (name.empty()) {
			m_scriptEngine->log("addAnimState: пустое имя состояния", ConsoleLogEntry::Error);
			return false;
		}
		if (anim->clips.find(clipName) == anim->clips.end()) {
			m_scriptEngine->log("addAnimState: клипа '" + clipName + "' нет в компоненте анимации",
			                    ConsoleLogEntry::Error);
			return false;
		}

		AnimState state;
		state.name = name;
		state.clipName = clipName;
		state.speed = speed.value_or(1.0f);
		state.loop = loop.value_or(true);

		const int existing = anim->graph.findState(name);
		if (existing >= 0) anim->graph.states[existing] = state;
		else               anim->graph.states.push_back(state);

		if (anim->graph.entryState.empty()) anim->graph.entryState = name;
		return true;
	};

	// from == "" означает «из любого состояния».
	// conditions — массив таблиц { param = "имя", op = "greater|less|equal|notequal|true|false|trigger", value = 0 }
	ut["addAnimTransition"] = [this, animOf](ObjectRef& ref, const std::string& from, const std::string& to,
	                                   sol::optional<float> blendTime, sol::optional<bool> waitForClipEnd,
	                                   sol::optional<sol::table> conditions) -> bool {
		auto* anim = animOf(ref); if (!anim) return false;

		// Состояния должны существовать на момент добавления перехода:
		// раньше опечатка в to молча давала переход, который никогда
		// не сработает, а опечатка в from — переход «из ниоткуда»
		if (!from.empty() && anim->graph.findState(from) < 0) {
			m_scriptEngine->log("addAnimTransition: состояния '" + from + "' нет",
			                    ConsoleLogEntry::Error);
			return false;
		}
		if (anim->graph.findState(to) < 0) {
			m_scriptEngine->log("addAnimTransition: состояния '" + to + "' нет",
			                    ConsoleLogEntry::Error);
			return false;
		}

		AnimTransition transition;
		transition.from = from;
		transition.to = to;
		transition.blendTime = blendTime.value_or(0.2f);
		transition.waitForClipEnd = waitForClipEnd.value_or(false);

		if (conditions) {
			for (auto& [key, value] : *conditions) {
				sol::table entry = value.as<sol::table>();
				AnimCondition cond;
				cond.parameter = entry.get_or("param", std::string());
				cond.value = entry.get_or("value", 0.0f);

				const std::string op = entry.get_or("op", std::string("true"));
				if      (op == "greater")  cond.op = AnimCondition::Op::Greater;
				else if (op == "less")     cond.op = AnimCondition::Op::Less;
				else if (op == "equal")    cond.op = AnimCondition::Op::Equal;
				else if (op == "notequal") cond.op = AnimCondition::Op::NotEqual;
				else if (op == "false")    cond.op = AnimCondition::Op::IsFalse;
				else if (op == "trigger")  cond.op = AnimCondition::Op::Trigger;
				else if (op == "true")     cond.op = AnimCondition::Op::IsTrue;
				else {
					// Раньше любая опечатка молча становилась IsTrue, то есть
					// «greatr» превращался в «параметр ≠ 0» и тихо менял логику
					m_scriptEngine->log("addAnimTransition: неизвестная операция '" + op +
					                    "' (есть greater, less, equal, notequal, true, false, trigger)",
					                    ConsoleLogEntry::Error);
					return false;
				}

				if (cond.parameter.empty()) {
					m_scriptEngine->log("addAnimTransition: у условия не задан param",
					                    ConsoleLogEntry::Error);
					return false;
				}

				transition.conditions.push_back(cond);
			}
		}
		anim->graph.transitions.push_back(transition);
		return true;
	};

	ut["setAnimEntryState"] = [animOf](ObjectRef& ref, const std::string& name) {
		if (auto* anim = animOf(ref)) anim->graph.entryState = name;
	};

	// Запускает граф с начального состояния
	ut["startAnimGraph"] = [animOf](ObjectRef& ref) {
		auto* anim = animOf(ref); if (!anim) return;
		anim->graph.enabled = true;
		anim->graph.currentState = -1;
		anim->graph.previousState = -1;
		anim->graph.blendProgress = 1.0f;
		anim->isPlaying = true;
		anim->isPaused = false;
	};

	ut["stopAnimGraph"] = [animOf](ObjectRef& ref) {
		if (auto* anim = animOf(ref)) anim->graph.enabled = false;
	};

	ut["setAnimParam"] = [animOf](ObjectRef& ref, const std::string& name, sol::object value) {
		auto* anim = animOf(ref); if (!anim) return;
		if (value.is<bool>())        anim->graph.params[name] = value.as<bool>() ? 1.0f : 0.0f;
		else if (value.is<double>()) anim->graph.params[name] = static_cast<float>(value.as<double>());
	};

	ut["getAnimParam"] = [animOf](ObjectRef& ref, const std::string& name) -> float {
		auto* anim = animOf(ref); if (!anim) return 0.0f;
		auto it = anim->graph.params.find(name);
		return it != anim->graph.params.end() ? it->second : 0.0f;
	};

	// Одноразовый флаг: гаснет сам, как только сработает переход
	ut["setAnimTrigger"] = [animOf](ObjectRef& ref, const std::string& name) {
		if (auto* anim = animOf(ref)) anim->graph.triggers[name] = true;
	};

	ut["getAnimState"] = [animOf](ObjectRef& ref) -> std::string {
		auto* anim = animOf(ref); if (!anim) return "";
		const int index = anim->graph.currentState;
		if (index < 0 || index >= static_cast<int>(anim->graph.states.size())) return "";
		return anim->graph.states[index].name;
	};

	// Прогресс кроссфейда: 1 = переход завершён
	ut["getAnimBlend"] = [animOf](ObjectRef& ref) -> float {
		auto* anim = animOf(ref);
		return anim ? anim->graph.blendProgress : 1.0f;
	};

	ut["clearAnimGraph"] = [animOf](ObjectRef& ref) {
		auto* anim = animOf(ref); if (!anim) return;
		anim->graph = AnimGraph{};
	};

	// Возвращают признак успеха: опечатка в имени клипа иначе молча не делала
	// ничего, и находилось это уже по «анимация не играет» в готовой игре.
	ut["addBoneTrack"] = [](ObjectRef& ref, const std::string& clipName,
	                        uint32_t trackId, uint32_t boneId) -> bool {
		GameObject* go = ref.get(); if (!go) return false;
		auto* anim = getComponent<AnimationComponent>(*go); if (!anim) return false;

		auto it = anim->clips.find(clipName);
		if (it == anim->clips.end()) return false;

		for (const auto& t : it->second.boneTracks)
			if (t.trackId == trackId && t.boneId == boneId) return true;   // уже есть

		it->second.boneTracks.push_back({trackId, boneId, {}});
		return true;
	};

	ut["addBoneKeyframe"] = [](ObjectRef& ref, const std::string& clipName, uint32_t trackId, uint32_t boneId,
	                           float time, const LuaVec3& pos, const LuaVec3& rot, const LuaVec3& scl) -> bool {
		GameObject* go = ref.get(); if (!go) return false;
		auto* anim = getComponent<AnimationComponent>(*go); if (!anim) return false;

		auto it = anim->clips.find(clipName);
		if (it == anim->clips.end()) return false;

		for (auto& track : it->second.boneTracks) {
			if (track.trackId != trackId || track.boneId != boneId) continue;

			BoneKeyframe kf;
			kf.time = time;
			kf.position = pos.toGlm();
			kf.rotation = rot.toGlm();
			kf.scale = scl.toGlm();
			track.keyframes.push_back(kf);

			std::sort(track.keyframes.begin(), track.keyframes.end(),
				[](const BoneKeyframe& a, const BoneKeyframe& b) { return a.time < b.time; });
			return true;
		}
		return false;   // трека с такой парой (trackId, boneId) нет
	};

	// Привязка меши к скелету
    ut["attachBody"] = [](ObjectRef& ref, uint32_t bodyObjId) {
        GameObject* go = ref.get(); if (!go) return;
        if (auto* skel = getComponent<SkeletonComponent>(*go)) {
            if (std::find(skel->attachedBodies.begin(), skel->attachedBodies.end(), bodyObjId) == skel->attachedBodies.end()) {
                skel->attachedBodies.push_back(bodyObjId);

                // Тело помечается как скиннингованное и запоминает свой скелет
                GameObject* bodyGo = ref.app->getObjectById(bodyObjId);
                if (bodyGo) {
                    if (auto* body = getComponent<MeshComponent>(*bodyGo)) {
                        body->isSkinnedMesh = true;
                        body->skeletonId = go->id;
                    }
                }
            }
        }
    };

    ut["detachBody"] = [](ObjectRef& ref, uint32_t bodyObjId) {
        GameObject* go = ref.get(); if (!go) return;
        if (auto* skel = getComponent<SkeletonComponent>(*go)) {
            auto& v = skel->attachedBodies;
            v.erase(std::remove(v.begin(), v.end(), bodyObjId), v.end());

            // Снимаем флаг и обратную ссылку
            GameObject* bodyGo = ref.app->getObjectById(bodyObjId);
            if (bodyGo) {
                if (auto* body = getComponent<MeshComponent>(*bodyGo)) {
                    body->isSkinnedMesh = false;
                    body->skeletonId = 0;
                }
            }
        }
    };

	// ============================================================
	// Доступ к костям скелета
	// ============================================================
	// Без него нельзя ни довернуть голову из скрипта, ни прицепить
	// оружие к кости руки — основные сценарии скелетной анимации.

	// ============================================================
	// Shape keys (morph targets)
	// ============================================================
	// Дельты форм лежат у меша, а веса — у объекта, поэтому один и тот же
	// .glb можно поставить в сцену дважды с разными выражениями лица.

	// Список имён форм в порядке, в котором их ждёт движок
	ut["shapeKeys"] = [this](ObjectRef& ref) -> sol::table {
		sol::table t = m_scriptEngine->lua().create_table();
		GameObject* go = ref.get(); if (!go) return t;
		auto* mesh = getComponent<MeshComponent>(*go); if (!mesh) return t;

		const auto& names = graphics->getShapeKeyNames(mesh->renderable);
		for (size_t i = 0; i < names.size(); ++i) t[i + 1] = names[i];
		return t;
	};

	ut["shapeKeyCount"] = [this](ObjectRef& ref) -> int {
		GameObject* go = ref.get(); if (!go) return 0;
		auto* mesh = getComponent<MeshComponent>(*go); if (!mesh) return 0;
		return static_cast<int>(graphics->getShapeKeyNames(mesh->renderable).size());
	};

	// Вес формы по имени. Неизвестное имя — 0, как будто форма не задействована
	ut["getShapeKey"] = [this](ObjectRef& ref, const std::string& name) -> float {
		GameObject* go = ref.get(); if (!go) return 0.0f;
		auto* mesh = getComponent<MeshComponent>(*go); if (!mesh) return 0.0f;

		const int idx = graphics->findShapeKey(mesh->renderable, name);
		if (idx < 0 || idx >= static_cast<int>(mesh->shapeKeyWeights.size())) return 0.0f;
		return mesh->shapeKeyWeights[idx];
	};

	// Задаёт вес формы. false — такой формы у меша нет (обычно опечатка
	// в имени), и молчать об этом нельзя: модель просто не шевельнётся.
	ut["setShapeKey"] = [this](ObjectRef& ref, const std::string& name, float weight) -> bool {
		GameObject* go = ref.get(); if (!go) return false;
		auto* mesh = getComponent<MeshComponent>(*go); if (!mesh) return false;

		const int idx = graphics->findShapeKey(mesh->renderable, name);
		if (idx < 0) return false;

		// Вектор весов заводим по факту первой правки: у большинства объектов
		// форм нет вовсе, и держать пустой вектор на каждом меше незачем
		const size_t need = static_cast<size_t>(idx) + 1;
		if (mesh->shapeKeyWeights.size() < need) mesh->shapeKeyWeights.resize(need, 0.0f);
		mesh->shapeKeyWeights[idx] = weight;
		return true;
	};

	// Возвращает все формы в ноль — базовая геометрия
	ut["clearShapeKeys"] = [](ObjectRef& ref) {
		GameObject* go = ref.get(); if (!go) return;
		if (auto* mesh = getComponent<MeshComponent>(*go)) mesh->shapeKeyWeights.clear();
	};

	ut["boneCount"] = [](ObjectRef& ref) -> int {
		GameObject* go = ref.get(); if (!go) return 0;
		auto* skel = getComponent<SkeletonComponent>(*go);
		return skel ? static_cast<int>(skel->bones.size()) : 0;
	};

	// Номер кости по имени; -1, если такой нет
	ut["findBone"] = [](ObjectRef& ref, const std::string& name) -> int {
		GameObject* go = ref.get(); if (!go) return -1;
		auto* skel = getComponent<SkeletonComponent>(*go); if (!skel) return -1;
		for (size_t i = 0; i < skel->bones.size(); ++i)
			if (skel->bones[i].name == name) return static_cast<int>(i);
		return -1;
	};

	// Таблица с описанием кости: имя, родитель и локальная трансформация
	ut["getBone"] = [this](ObjectRef& ref, int index) -> sol::object {
		GameObject* go = ref.get(); if (!go) return sol::nil;
		auto* skel = getComponent<SkeletonComponent>(*go); if (!skel) return sol::nil;
		if (index < 0 || index >= static_cast<int>(skel->bones.size())) return sol::nil;

		const Bone& bone = skel->bones[index];
		sol::table t = m_scriptEngine->lua().create_table();
		t["name"]          = bone.name;
		t["parent"]        = (bone.parentId == Bone::NO_BONE_PARENT) ? -1 : static_cast<int>(bone.parentId);
		t["localPosition"] = LuaVec3(bone.localPosition);
		t["localRotation"] = LuaVec3(bone.localRotation);
		t["localScale"]    = LuaVec3(bone.localScale);
		return sol::make_object(m_scriptEngine->lua(), t);
	};

	// ------------------------------------------------------------------
	// Кости под управлением физики (регдолл со скиннингом)
	// ------------------------------------------------------------------
	// С момента привязки позу кости задаёт тело, а не иерархия и не анимация.
	// Поправка считается по текущей позе, поэтому персонаж не «дёргается»
	// в момент перехода в регдолл.
	ut["driveBoneFromBody"] = [this](ObjectRef& ref, const std::string& boneName,
	                                 int bodyHandle) -> bool {
		GameObject* go = ref.get(); if (!go) return false;
		auto* skel = getComponent<SkeletonComponent>(*go);
		if (!skel) {
			m_scriptEngine->log("driveBoneFromBody: у объекта нет скелета", ConsoleLogEntry::Error);
			return false;
		}
		if (!driveBoneFromBody(*go, *skel, boneName, static_cast<PhysicsHandle>(bodyHandle))) {
			m_scriptEngine->log("driveBoneFromBody: кости '" + boneName + "' нет в скелете",
			                    ConsoleLogEntry::Error);
			return false;
		}
		return true;
	};

	// Возвращает кость под управление иерархии и анимации
	ut["releaseBone"] = [this](ObjectRef& ref, const std::string& boneName) -> bool {
		GameObject* go = ref.get(); if (!go) return false;
		auto* skel = getComponent<SkeletonComponent>(*go);
		if (!skel) return false;
		return releaseBoneFromBody(*skel, boneName);
	};

	// Все ли кости отпущены (удобно понять, вышел ли персонаж из регдолла)
	ut["hasPhysicsBones"] = [](ObjectRef& ref) -> bool {
		GameObject* go = ref.get(); if (!go) return false;
		auto* skel = getComponent<SkeletonComponent>(*go);
		return skel && skel->hasPhysicsDrivenBones();
	};

	// Задаёт локальную позу кости. Поза пересчитается в ближайшем кадре.
	ut["setBonePose"] = [](ObjectRef& ref, int index, const LuaVec3& pos,
	                       const LuaVec3& rot, sol::optional<LuaVec3> scale) -> bool {
		GameObject* go = ref.get(); if (!go) return false;
		auto* skel = getComponent<SkeletonComponent>(*go); if (!skel) return false;
		if (index < 0 || index >= static_cast<int>(skel->bones.size())) return false;

		Bone& bone = skel->bones[index];
		bone.localPosition = pos.toGlm();
		bone.localRotation = rot.toGlm();
		if (scale) bone.localScale = scale->toGlm();
		skel->isDirty = true;
		return true;
	};

	// Позиция кости в мире: поза кости внутри объекта плюс трансформация объекта.
	// По ней прикрепляют предметы к рукам и головам.
	ut["getBoneWorldPosition"] = [](ObjectRef& ref, int index) -> LuaVec3 {
		GameObject* go = ref.get(); if (!go) return LuaVec3(0);
		auto* skel = getComponent<SkeletonComponent>(*go); if (!skel) return LuaVec3(0);
		if (index < 0 || index >= static_cast<int>(skel->boneGlobalPose.size())) return LuaVec3(0);

		const glm::mat4 objectMat = glm::translate(glm::mat4(1.0f), go->position) *
		                            glm::mat4_cast(go->rotationQuat) *
		                            glm::scale(glm::mat4(1.0f), go->scale);
		return LuaVec3(glm::vec3((objectMat * skel->boneGlobalPose[index])[3]));
	};

	// Поворот кости в мире в градусах
	ut["getBoneWorldRotation"] = [](ObjectRef& ref, int index) -> LuaVec3 {
		GameObject* go = ref.get(); if (!go) return LuaVec3(0);
		auto* skel = getComponent<SkeletonComponent>(*go); if (!skel) return LuaVec3(0);
		if (index < 0 || index >= static_cast<int>(skel->boneGlobalPose.size())) return LuaVec3(0);

		const glm::mat4 world = glm::mat4_cast(go->rotationQuat) * skel->boneGlobalPose[index];
		glm::vec3 columns(glm::length(glm::vec3(world[0])),
		                  glm::length(glm::vec3(world[1])),
		                  glm::length(glm::vec3(world[2])));
		glm::mat3 rotOnly(glm::vec3(world[0]) / std::max(columns.x, 1e-8f),
		                  glm::vec3(world[1]) / std::max(columns.y, 1e-8f),
		                  glm::vec3(world[2]) / std::max(columns.z, 1e-8f));
		return LuaVec3(glm::degrees(glm::eulerAngles(glm::quat_cast(rotOnly))));
	};


    // ============================================================
    // ParticleEmitter-специфичные свойства (Roblox-style!)
    // ============================================================
    ut["rate"] = sol::property(
        [](ObjectRef& ref) -> sol::object {
            GameObject* go = ref.get(); if (!go) return sol::object();
            if (auto* emitter = getComponent<ParticleEmitterComponent>(*go))
                return sol::make_object(ref.app->m_scriptEngine->lua(), emitter->rate);
            return sol::object();
        },
        [](ObjectRef& ref, float val) {
            GameObject* go = ref.get(); if (!go) return;
            if (auto* emitter = getComponent<ParticleEmitterComponent>(*go)) emitter->rate = val;
        }
    );

	ut["blendMode"] = sol::property(
		[](ObjectRef& ref) -> int {
			GameObject* go = ref.get(); if (!go) return 0;
			if (auto* emitter = getComponent<ParticleEmitterComponent>(*go)) return static_cast<int>(emitter->blendMode);
			if (auto* sp = getComponent<SingleParticleComponent>(*go)) return static_cast<int>(sp->blendMode);
			return 0;
		},
		[](ObjectRef& ref, int val) {
			GameObject* go = ref.get(); if (!go) return;
			auto mode = static_cast<ParticleBlendMode>(val);
			if (auto* emitter = getComponent<ParticleEmitterComponent>(*go)) emitter->blendMode = mode;
			else if (auto* sp = getComponent<SingleParticleComponent>(*go)) sp->blendMode = mode;
		}
	);

    ut["lifetime"] = sol::property(
        [](ObjectRef& ref) -> sol::object {
            GameObject* go = ref.get(); if (!go) return sol::object();
            if (auto* emitter = getComponent<ParticleEmitterComponent>(*go))
                return sol::make_object(ref.app->m_scriptEngine->lua(), emitter->lifetime);
            return sol::object();
        },
        [](ObjectRef& ref, const LuaVec2& val) {
            GameObject* go = ref.get(); if (!go) return;
            if (auto* emitter = getComponent<ParticleEmitterComponent>(*go)) emitter->lifetime = val;
        }
    );

    ut["speed"] = sol::property(
        [](ObjectRef& ref) -> sol::object {
            GameObject* go = ref.get(); if (!go) return sol::object();
            if (auto* emitter = getComponent<ParticleEmitterComponent>(*go))
                return sol::make_object(ref.app->m_scriptEngine->lua(), emitter->speed);
            return sol::object();
        },
        [](ObjectRef& ref, const LuaVec2& val) {
            GameObject* go = ref.get(); if (!go) return;
            if (auto* emitter = getComponent<ParticleEmitterComponent>(*go)) emitter->speed = val;
        }
    );

    ut["spreadAngle"] = sol::property(
        [](ObjectRef& ref) -> sol::object {
            GameObject* go = ref.get(); if (!go) return sol::object();
            if (auto* emitter = getComponent<ParticleEmitterComponent>(*go))
                return sol::make_object(ref.app->m_scriptEngine->lua(), emitter->spreadAngle);
            return sol::object();
        },
        [](ObjectRef& ref, const LuaVec2& val) {
            GameObject* go = ref.get(); if (!go) return;
            if (auto* emitter = getComponent<ParticleEmitterComponent>(*go)) emitter->spreadAngle = val;
        }
    );

    ut["startRotation"] = sol::property(
        [](ObjectRef& ref) -> sol::object {
            GameObject* go = ref.get(); if (!go) return sol::object();
            if (auto* emitter = getComponent<ParticleEmitterComponent>(*go))
                return sol::make_object(ref.app->m_scriptEngine->lua(), emitter->startRotation);
            return sol::object();
        },
        [](ObjectRef& ref, const LuaVec2& val) {
            GameObject* go = ref.get(); if (!go) return;
            if (auto* emitter = getComponent<ParticleEmitterComponent>(*go)) emitter->startRotation = val;
        }
    );

    ut["rotationSpeed"] = sol::property(
        [](ObjectRef& ref) -> sol::object {
            GameObject* go = ref.get(); if (!go) return sol::object();
            if (auto* emitter = getComponent<ParticleEmitterComponent>(*go))
                return sol::make_object(ref.app->m_scriptEngine->lua(), emitter->rotationSpeed);
            return sol::object();
        },
        [](ObjectRef& ref, const LuaVec2& val) {
            GameObject* go = ref.get(); if (!go) return;
            if (auto* emitter = getComponent<ParticleEmitterComponent>(*go)) emitter->rotationSpeed = val;
        }
    );

	ut["brightness"] = sol::property(
		[](ObjectRef& ref) -> sol::object {
			GameObject* go = ref.get(); if (!go) return sol::object();
			if (auto* emitter = getComponent<ParticleEmitterComponent>(*go))
				return sol::make_object(ref.app->m_scriptEngine->lua(), emitter->brightness);
			if (auto* sp = getComponent<SingleParticleComponent>(*go))
				return sol::make_object(ref.app->m_scriptEngine->lua(), sp->brightness);
			return sol::object();
		},
		[](ObjectRef& ref, sol::object val) {
			GameObject* go = ref.get(); if (!go) return;
			if (auto* emitter = getComponent<ParticleEmitterComponent>(*go)) {
				if (val.is<LuaVec2>()) emitter->brightness = val.as<LuaVec2>();
			} else if (auto* sp = getComponent<SingleParticleComponent>(*go)) {
				if (val.is<double>() || val.is<float>()) sp->brightness = static_cast<float>(val.as<double>());
			}
		}
	);

	ut["opacity"] = sol::property(
		[](ObjectRef& ref) -> sol::object {
			GameObject* go = ref.get(); if (!go) return sol::object();
			if (auto* emitter = getComponent<ParticleEmitterComponent>(*go))
				return sol::make_object(ref.app->m_scriptEngine->lua(), emitter->opacity);
			if (auto* sp = getComponent<SingleParticleComponent>(*go))
				return sol::make_object(ref.app->m_scriptEngine->lua(), sp->opacity);
			return sol::object();
		},
		[](ObjectRef& ref, sol::object val) {
			GameObject* go = ref.get(); if (!go) return;
			if (auto* emitter = getComponent<ParticleEmitterComponent>(*go)) {
				if (val.is<LuaVec2>()) emitter->opacity = val.as<LuaVec2>();
			} else if (auto* sp = getComponent<SingleParticleComponent>(*go)) {
				if (val.is<double>() || val.is<float>()) sp->opacity = static_cast<float>(val.as<double>());
			}
		}
	);

	ut["color"] = sol::property(
		[](ObjectRef& ref) -> sol::object {
			GameObject* go = ref.get(); if (!go) return sol::object();
			if (auto* light = getComponent<LightComponent>(*go))
				return sol::make_object(ref.app->m_scriptEngine->lua(), LuaVec3(light->color));
			if (auto* emitter = getComponent<ParticleEmitterComponent>(*go))
				return sol::make_object(ref.app->m_scriptEngine->lua(), emitter->color);
			if (auto* sp = getComponent<SingleParticleComponent>(*go))
				return sol::make_object(ref.app->m_scriptEngine->lua(), LuaVec4(sp->color));
			if (auto* cs = getComponent<ConstraintComponent>(*go))
				return sol::make_object(ref.app->m_scriptEngine->lua(), LuaVec4(cs->color));
			return sol::object();
		},
		[](ObjectRef& ref, sol::object val) {
			GameObject* go = ref.get(); if (!go) return;
			if (auto* light = getComponent<LightComponent>(*go)) {
				if (val.is<LuaVec3>()) light->color = val.as<LuaVec3>().toGlm();
			} else if (auto* emitter = getComponent<ParticleEmitterComponent>(*go)) {
				if (val.is<LuaGradient>()) emitter->color = val.as<LuaGradient>();
			} else if (auto* sp = getComponent<SingleParticleComponent>(*go)) {
				if (val.is<LuaVec4>()) sp->color = val.as<LuaVec4>().toGlm();
			} else if (auto* cs = getComponent<ConstraintComponent>(*go)) {
				if (val.is<LuaVec4>()) cs->color = val.as<LuaVec4>().toGlm();
			}
		}
	);

	ut["size"] = sol::property(
		[](ObjectRef& ref) -> sol::object {
			GameObject* go = ref.get(); if (!go) return sol::object();
			if (auto* emitter = getComponent<ParticleEmitterComponent>(*go))
				return sol::make_object(ref.app->m_scriptEngine->lua(), emitter->size);
			if (auto* sp = getComponent<SingleParticleComponent>(*go))
				return sol::make_object(ref.app->m_scriptEngine->lua(), sp->size);
			return sol::object();
		},
		[](ObjectRef& ref, sol::object val) {
			GameObject* go = ref.get(); if (!go) return;
			if (auto* emitter = getComponent<ParticleEmitterComponent>(*go)) {
				if (val.is<LuaNumSequence>()) emitter->size = val.as<LuaNumSequence>();
			} else if (auto* sp = getComponent<SingleParticleComponent>(*go)) {
				if (val.is<double>() || val.is<float>()) sp->size = static_cast<float>(val.as<double>());
			}
		}
	);

	ut["particleRotation"] = sol::property(
		[](ObjectRef& ref) -> float {
			GameObject* go = ref.get();
			if (auto* sp = go ? getComponent<SingleParticleComponent>(*go) : nullptr) return sp->rotation;
			return 0.0f;
		},
		[](ObjectRef& ref, float val) {
			GameObject* go = ref.get();
			if (auto* sp = go ? getComponent<SingleParticleComponent>(*go) : nullptr) sp->rotation = val;
		}
	);

    ut["direction"] = sol::property(
        [](ObjectRef& ref) -> sol::object {
            GameObject* go = ref.get(); if (!go) return sol::object();
            if (auto* emitter = getComponent<ParticleEmitterComponent>(*go))
                return sol::make_object(ref.app->m_scriptEngine->lua(), LuaVec3(emitter->direction));
            return sol::object();
        },
        [](ObjectRef& ref, const LuaVec3& val) {
            GameObject* go = ref.get(); if (!go) return;
            if (auto* emitter = getComponent<ParticleEmitterComponent>(*go)) emitter->direction = val.toGlm();
        }
    );

    ut["gravity"] = sol::property(
        [](ObjectRef& ref) -> sol::object {
            GameObject* go = ref.get(); if (!go) return sol::object();
            if (auto* emitter = getComponent<ParticleEmitterComponent>(*go))
                return sol::make_object(ref.app->m_scriptEngine->lua(), LuaVec3(emitter->gravity));
            return sol::object();
        },
        [](ObjectRef& ref, const LuaVec3& val) {
            GameObject* go = ref.get(); if (!go) return;
            if (auto* emitter = getComponent<ParticleEmitterComponent>(*go)) emitter->gravity = val.toGlm();
        }
    );

    ut["emit"] = [](ObjectRef& ref, int count) {
        GameObject* go = ref.get(); if (!go) return;
        if (auto* emitter = getComponent<ParticleEmitterComponent>(*go)) {
            emitter->spawnAccumulator += static_cast<float>(count);
            emitter->isPlaying = true;
        }
    };

	ut["texture"] = sol::property(
		[](ObjectRef& ref) -> std::string {
			GameObject* go = ref.get(); if (!go) return "";
			if (auto* emitter = getComponent<ParticleEmitterComponent>(*go)) return emitter->textureName;
			if (auto* sp = getComponent<SingleParticleComponent>(*go)) return sp->textureName;
			if (auto* c = getComponent<ConstraintComponent>(*go)) return c->textureName;
			return "";
		},
		[this](ObjectRef& ref, const std::string& name) {
			GameObject* go = ref.get(); if (!go) return;
			if (auto* emitter = getComponent<ParticleEmitterComponent>(*go)) {
				emitter->textureName = name;
				emitter->textureHandle = graphics->getTextureHandleByName(name);
				if (emitter->textureHandle == INVALID_HANDLE) m_scriptEngine->log("Particle texture not found: " + name, ConsoleLogEntry::Warning);
			} else if (auto* sp = getComponent<SingleParticleComponent>(*go)) {
				sp->textureName = name;
				sp->textureHandle = graphics->getTextureHandleByName(name);
			} else if (auto* c = getComponent<ConstraintComponent>(*go)) {
				c->textureName = name;
				c->textureHandle = graphics->getTextureHandleByName(name);
			}
		}
	);

	ut["normalmap"] = sol::property(
		[](ObjectRef& ref) -> std::string {
			GameObject* go = ref.get(); if (!go) return "";
			if (auto* emitter = getComponent<ParticleEmitterComponent>(*go)) return emitter->nmapName;
			if (auto* sp = getComponent<SingleParticleComponent>(*go)) return sp->nmapName;
			if (auto* c = getComponent<ConstraintComponent>(*go)) return c->nmapName;
			return "";
		},
		[this](ObjectRef& ref, const std::string& name) {
			GameObject* go = ref.get(); if (!go) return;
			if (auto* emitter = getComponent<ParticleEmitterComponent>(*go)) {
				emitter->nmapName = name;
				emitter->nmapHandle = graphics->getTextureHandleByName(name);
			} else if (auto* sp = getComponent<SingleParticleComponent>(*go)) {
				sp->nmapName = name;
				sp->nmapHandle = graphics->getTextureHandleByName(name);
			} else if (auto* c = getComponent<ConstraintComponent>(*go)) {
				c->nmapName = name;
				c->nmapHandle = graphics->getTextureHandleByName(name);
			}
		}
	);

	ut["materialName"] = sol::property(
		[](ObjectRef& ref) -> std::string {
			GameObject* go = ref.get(); if (!go) return "";
			if (auto* emitter = getComponent<ParticleEmitterComponent>(*go)) return emitter->textureName;
			if (auto* sp = getComponent<SingleParticleComponent>(*go)) return sp->textureName;
			if (auto* c = getComponent<ConstraintComponent>(*go)) return c->textureName;
			return "";
		},
		[this](ObjectRef& ref, const std::string& name) {
			GameObject* go = ref.get(); if (!go) return;

			auto applyMaterial = [this](auto* target, const std::string& matName) {
				MaterialHandle matHandle = getMaterialHandleByName(matName);
				if (matHandle != INVALID_HANDLE) {
					Material mat = graphics->getMaterial(matHandle);
					target->textureHandle = mat.albedoMap;
					if (target->textureHandle == INVALID_HANDLE) {
						m_scriptEngine->log("Material '" + matName + "' has no albedo texture!", ConsoleLogEntry::Warning);
					}
					target->nmapHandle = mat.normalMap;
					if (target->nmapHandle == INVALID_HANDLE) {
						m_scriptEngine->log("Material '" + matName + "' has no nmap texture!", ConsoleLogEntry::Warning);
					}
				} else {
					target->textureHandle = INVALID_HANDLE;
					target->nmapHandle = INVALID_HANDLE;
					m_scriptEngine->log("Material not found: " + matName, ConsoleLogEntry::Warning);
				}
			};

			if (auto* emitter = getComponent<ParticleEmitterComponent>(*go)) {
				emitter->textureName = name;
				applyMaterial(emitter, name);
			} else if (auto* sp = getComponent<SingleParticleComponent>(*go)) {
				sp->textureName = name;
				applyMaterial(sp, name);
			} else if (auto* c = getComponent<ConstraintComponent>(*go)) {
				c->textureName = name;
				applyMaterial(c, name);
			}
		}
	);

    ut["flipbookGrid"] = sol::property(
        [](ObjectRef& ref) -> sol::object {
            GameObject* go = ref.get(); if (!go) return sol::object();
            if (auto* emitter = getComponent<ParticleEmitterComponent>(*go))
                return sol::make_object(ref.app->m_scriptEngine->lua(), LuaVec2(emitter->flipbookGridX, emitter->flipbookGridY));
            return sol::object();
        },
        [](ObjectRef& ref, const LuaVec2& val) {
            GameObject* go = ref.get(); if (!go) return;
            if (auto* emitter = getComponent<ParticleEmitterComponent>(*go)) {
                // Не меньше единицы: сетка кадров идёт делителем при выборе
                // кадра, и flipbookGrid = Vector2(0, 1) роняло движок по SIGFPE.
                // Редактор клампит давно, Lua — нет.
                emitter->flipbookGridX = std::max(1, static_cast<int>(val.x));
                emitter->flipbookGridY = std::max(1, static_cast<int>(val.y));
            }
        }
    );

    ut["flipbookFPS"] = sol::property(
        [](ObjectRef& ref) -> sol::object {
            GameObject* go = ref.get(); if (!go) return sol::object();
            if (auto* emitter = getComponent<ParticleEmitterComponent>(*go))
                return sol::make_object(ref.app->m_scriptEngine->lua(), emitter->flipbookFPS);
            return sol::object();
        },
        [](ObjectRef& ref, float val) {
            GameObject* go = ref.get(); if (!go) return;
            if (auto* emitter = getComponent<ParticleEmitterComponent>(*go)) emitter->flipbookFPS = val;
        }
    );

    ut["randomStartFrame"] = sol::property(
        [](ObjectRef& ref) -> sol::object {
            GameObject* go = ref.get(); if (!go) return sol::object();
            if (auto* emitter = getComponent<ParticleEmitterComponent>(*go))
                return sol::make_object(ref.app->m_scriptEngine->lua(), emitter->randomStartFrame);
            return sol::object();
        },
        [](ObjectRef& ref, bool val) {
            GameObject* go = ref.get(); if (!go) return;
            if (auto* emitter = getComponent<ParticleEmitterComponent>(*go)) emitter->randomStartFrame = val;
        }
    );

	// ====================================================================
	// ЧТЕНИЕ неизвестных свойств (Дети + CustomComponent)
	// ====================================================================
	ut[sol::meta_function::index] = [this](ObjectRef& ref, const std::string& key, sol::this_state s) -> sol::object {
		GameObject* go = ref.get();
		if (!go) return sol::nil;

		// 1. Сначала проверяем, не является ли это свойством CustomComponent
		auto* custom = getComponent<CustomComponent>(*go);
		if (custom) {
			auto it = custom->params.find(key);
			if (it != custom->params.end()) {
				return std::visit([&](auto&& arg) -> sol::object {
					using T = std::decay_t<decltype(arg)>;
					if constexpr (std::is_same_v<T, float> || std::is_same_v<T, bool> || std::is_same_v<T, std::string>) {
						return sol::make_object(s, arg);
					}
					return sol::nil;
				}, it->second);
			}
		}

		// 2. Если не CustomComponent, ищем ребенка по имени (двухуровневый поиск)
		GameObject* child = findChildObject(go, key);
		if (child) {
			return sol::make_object(s, ObjectRef{child->id, ref.app});
		}

		// 3. Ничего не найдено -> возвращаем nil
		return sol::nil;
	};

	// ====================================================================
	// ЗАПИСЬ в неизвестные свойства (Только для CustomComponent)
	// ====================================================================
	ut[sol::meta_function::new_index] = [this](ObjectRef& ref, const std::string& key, sol::object value, sol::this_state s) {
		(void)s;
		GameObject* go = ref.get();
		if (!go) return;

		// Разрешаем запись только в CustomComponent
		auto* custom = getComponent<CustomComponent>(*go);
		if (!custom) {
			m_scriptEngine->log("Cannot set property '" + key + "' on non-custom object: " + go->name, ConsoleLogEntry::Warning);
			return;
		}

		// Сохраняем значение в зависимости от типа
		if (value.is<double>() || value.is<int>()) {
			custom->params[key] = static_cast<float>(value.as<double>());
		} else if (value.is<std::string>()) {
			custom->params[key] = value.as<std::string>();
		} else if (value.is<bool>()) {
			custom->params[key] = value.as<bool>();
		} else {
			m_scriptEngine->log("Unsupported type for custom property '" + key + "' on object: " + go->name, ConsoleLogEntry::Warning);
		}
	};

	bindComponentAPI();

	// ============================================================
	// Работа с компонентами объекта
	// ============================================================
	// Плоские свойства выше (obj.mass, obj.intensity, ...) всегда обращаются к
	// ПЕРВОМУ компоненту нужного типа. Методы ниже дают доступ к остальным:
	// объект может нести несколько источников света, звуков или тел, у каждого
	// свой локальный сдвиг и флаг включения.

	// Создаёт компонент указанного типа и возвращает ссылку на него
	ut["addComponent"] = [this](ObjectRef& ref, const std::string& type) -> sol::object {
		GameObject* go = ref.get();
		if (!go) return sol::nil;

		// Таблица типов одна на весь движок (makeComponentByName в GameObject.h):
		// её же использует кнопка «Добавить компонент» в редакторе
		const int index = addComponentByName(*go, type);
		if (index < 0) {
			m_scriptEngine->log("addComponent: неизвестный тип компонента '" + type + "'",
			                    ConsoleLogEntry::Error);
			return sol::nil;
		}
		return sol::make_object(m_scriptEngine->lua(),
		                        ComponentRef{go->id, index, go->components[index].uid, this});
	};

	// Компонент указанного типа; number — порядковый номер среди однотипных (с 1)
	ut["getComponent"] = [this](ObjectRef& ref, const std::string& type,
	                            sol::optional<int> number) -> sol::object {
		GameObject* go = ref.get();
		if (!go) return sol::nil;

		int wanted = number.value_or(1);
		for (size_t i = 0; i < go->components.size(); ++i) {
			if (go->components[i].getTypeName() != type) continue;
			if (--wanted > 0) continue;
			return sol::make_object(m_scriptEngine->lua(),
			                        ComponentRef{go->id, static_cast<int>(i),
			                                     go->components[i].uid, this});
		}
		return sol::nil;
	};

	// Все компоненты объекта, либо только указанного типа
	ut["getComponents"] = [this](ObjectRef& ref, sol::optional<std::string> type) -> sol::table {
		sol::table result = m_scriptEngine->lua().create_table();
		GameObject* go = ref.get();
		if (!go) return result;

		int idx = 1;
		for (size_t i = 0; i < go->components.size(); ++i) {
			if (type && go->components[i].getTypeName() != *type) continue;
			result[idx++] = ComponentRef{go->id, static_cast<int>(i),
			                             go->components[i].uid, this};
		}
		return result;
	};

	// Удаляет компонент (по ссылке или первый компонент указанного типа)
	ut["removeComponent"] = [this](ObjectRef& ref, sol::object arg) -> bool {
		GameObject* go = ref.get();
		if (!go) return false;

		int index = -1;
		if (arg.is<ComponentRef>()) {
			const ComponentRef comp = arg.as<ComponentRef>();
			if (comp.objectId != go->id) return false;
			index = comp.index;
		} else if (arg.is<std::string>()) {
			const std::string type = arg.as<std::string>();
			for (size_t i = 0; i < go->components.size(); ++i)
				if (go->components[i].getTypeName() == type) { index = static_cast<int>(i); break; }
		}

		if (index < 0 || index >= static_cast<int>(go->components.size())) return false;
		// Через removeComponentAt, а не erase: иначе тело осталось бы в
		// симуляции невидимым препятствием, а звук играл бы в пустоте
		return removeComponentAt(*go, static_cast<size_t>(index));
	};

	ut["componentCount"] = [](ObjectRef& ref) -> int {
		GameObject* go = ref.get();
		return go ? static_cast<int>(go->components.size()) : 0;
	};

	ut["componentTypes"] = [this](ObjectRef& ref) -> sol::table {
		sol::table result = m_scriptEngine->lua().create_table();
		GameObject* go = ref.get();
		if (!go) return result;
		for (size_t i = 0; i < go->components.size(); ++i)
			result[i + 1] = go->components[i].getTypeName();
		return result;
	};

    // ============================================================
    // Глобальная таблица Object
    // ============================================================
    sol::table objTable = lua.create_named_table("Object");

	objTable["updateChildren"] = [this](sol::object objArg) {
		GameObject* go = resolveObjectArg(objArg);
		if (go) {
			updateChildrenVector(go->id);
		}
	};

    objTable["new"] = [this](const std::string& type, sol::optional<std::string> name) -> ObjectRef {
        uint32_t id = addGameObject(type, name.value_or(""));
        return ObjectRef{id, this};
    };

	objTable["find"] = [this](sol::object arg) -> sol::object {
		GameObject* go = resolveObjectArg(arg);
		if (go) return sol::make_object(m_scriptEngine->lua(), ObjectRef{go->id, this});
		return sol::object();
	};

    objTable["destroy"] = [this](sol::object arg) {
        if (arg.is<ObjectRef>()) removeGameObject(arg.as<ObjectRef>().id);
        else if (arg.is<uint32_t>()) removeGameObject(arg.as<uint32_t>());
    };

    objTable["list"] = [this]() -> sol::table {
        sol::table result = m_scriptEngine->lua().create_table();
        int idx = 1;
        for (auto& go : gameObjects) result[idx++] = ObjectRef{go.id, this};
        return result;
    };

	objTable["buildGltf"] = [this](const std::string& baseName, const std::string& meshName, sol::optional<std::string> matName) -> sol::object {
		uint32_t id = buildGltf(baseName, meshName, matName.value_or(""));
		if (id == 0) return sol::nil;
		return sol::make_object(m_scriptEngine->lua(), ObjectRef{id, this});
	};

	// Создаёт скелет из меша с костями и привязывает к нему все его сабмеши
	objTable["newSkeleton"] = [this](const std::string& meshName, const std::string& materialName) -> sol::object {
        uint32_t id = addSkeletonObject(meshName, materialName);
        if (id == 0) return sol::nil;   // меш не найден или у него нет скелета
        return sol::make_object(m_scriptEngine->lua(), ObjectRef{id, this});
    };
}


