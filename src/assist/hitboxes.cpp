#include "hitboxes.hpp"

#include <Geode/Geode.hpp>

#include "bot/bot.hpp"
#include "ccTypes.h"
#include "settings/settings.hpp"
#include "trajectory/trajectory.hpp"

#define CC_COLOR(color_type)                \
    *reinterpret_cast<cocos2d::ccColor4F*>( \
        settings.categories[color_type].colors.data())
#define CC_FILL_COLOR(color_type)                               \
    {                                                           \
        settings.categories[color_type].colors.data()[0],       \
        settings.categories[color_type].colors.data()[1],       \
        settings.categories[color_type].colors.data()[2],       \
        settings.categories[color_type].colors.data()[3] *      \
            (float)settings.categories[color_type].fillOpacity, \
    }
#define HB_ENABLED(_t) settings.categories[_t].enabled

static cocos2d::CCRect usingWidth(const cocos2d::CCRect& old,
                                  const float width) {
    if (width == 0.0) return old;

    cocos2d::CCRect rect = old;
    rect.origin.x += width;
    rect.origin.y += width;
    rect.size.width -= width * 2.0;
    rect.size.height -= width * 2.0;
    return rect;
}

static void drawPlayerHitbox(cocos2d::CCDrawNode* node, GJBaseGameLayer* pl,
                             PlayerObject* player,
                             SLSettings::HitboxSettings& settings) {
    using Type = SLSettings::HitboxSettings::Type;
    if (!player) return;

    const float width = settings.width / pl->m_gameState.m_cameraZoom;

    auto rect = usingWidth(player->getObjectRect(), width);
    auto scaled = usingWidth(player->getObjectRect(0.3, 0.3), width);

    if (auto ob = player->getOrientedBox();
        ob && HB_ENABLED(Type::PlayerRotated)) {
        node->drawPolygon(ob->m_corners.data(), 4,
                          CC_FILL_COLOR(Type::PlayerRotated), width,
                          CC_COLOR(Type::PlayerRotated));
    }

    float radius = player->getObjectRect().size.width / 2.0;
    if (radius > 0 && HB_ENABLED(Type::PlayerCircle)) {
        int segments = (int)std::max(radius, 8.0f) * 3.0;

        node->drawCircle(player->getPosition(), radius, {0.0, 0.0, 0.0, 0.0},
                         width, CC_COLOR(Type::PlayerCircle), segments);
    }

    if (HB_ENABLED(Type::Player)) {
        node->drawRect(rect, CC_FILL_COLOR(Type::Player), width,
                       CC_COLOR(Type::Player));
    }
    if (HB_ENABLED(Type::PlayerInner)) {
        node->drawRect(scaled, CC_FILL_COLOR(Type::PlayerInner), width,
                       CC_COLOR(Type::PlayerInner));
    }
}

inline bool HitboxTrailUnit::shouldDraw(float minX, float maxX, float minY,
                                        float maxY) {
    if (m_rect.getMaxX() < minX || m_rect.getMinX() > maxX ||
        m_rect.getMinY() > maxY || m_rect.getMaxY() < minY)
        return false;

    return true;
}

void HitboxTrailUnit::draw(cocos2d::CCDrawNode* node, float width,
                           float* colors, float fillOpacity) {
    cocos2d::CCRect rect = usingWidth(m_rect, width);

    node->drawRect(rect,
                   {colors[0], colors[1], colors[2], colors[3] * fillOpacity},
                   width, *reinterpret_cast<cocos2d::ccColor4F*>(colors));
}

void HitboxTrailUnit::drawRotated(cocos2d::CCDrawNode* node, float width,
                                  float* colors, float fillOpacity) {
    node->drawPolygon(
        m_rotated.data(), 4,
        {colors[0], colors[1], colors[2], colors[3] * fillOpacity}, width,
        *reinterpret_cast<cocos2d::ccColor4F*>(colors));
}

void HitboxTrailUnit::drawInner(cocos2d::CCDrawNode* node, float width,
                                float* colors, float fillOpacity) {
    cocos2d::CCRect scaled = usingWidth(m_scaled, width);
    node->drawRect(scaled,
                   {colors[0], colors[1], colors[2], colors[3] * fillOpacity},
                   width, *reinterpret_cast<cocos2d::ccColor4F*>(colors));
}

void HitboxTrailUnit::drawCircle(cocos2d::CCDrawNode* node, float width,
                                 float* colors, float fillOpacity) {
    cocos2d::CCRect rect = usingWidth(m_rect, width);
    float radius = rect.size.width / 2.0;
    if (radius > 0) {
        int segments = (int)std::max(radius, 8.0f) * 3.0;

        node->drawCircle(
            cocos2d::CCPoint{rect.getMidX(), rect.getMidY()}, radius,
            {colors[0], colors[1], colors[2], colors[3] * fillOpacity}, width,
            *reinterpret_cast<cocos2d::ccColor4F*>(colors), segments);
    }
}

void Hitboxes::init(GJBaseGameLayer* pl) {
    if (m_initialized) {
        this->destroy();  // probably worth it to just recreate
    }

    m_drawNode = HitboxesDrawNode::create();
    m_drawNode->retain();
    m_drawNode->setID("hitbox-node"_spr);
    m_drawNode->setBlendFunc({GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA});

    m_solidTrailDrawNode = HitboxesDrawNode::create();
    m_solidTrailDrawNode->retain();
    m_solidTrailDrawNode->setID("hitbox-trail-node-solid"_spr);
    m_solidTrailDrawNode->setBlendFunc({GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA});

    m_circleTrailDrawNode = HitboxesDrawNode::create();
    m_circleTrailDrawNode->retain();
    m_circleTrailDrawNode->setID("hitbox-trail-node-circle"_spr);
    m_circleTrailDrawNode->setBlendFunc({GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA});

    m_innerTrailDrawNode = HitboxesDrawNode::create();
    m_innerTrailDrawNode->retain();
    m_innerTrailDrawNode->setID("hitbox-trail-node-inner"_spr);
    m_innerTrailDrawNode->setBlendFunc({GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA});

    m_rotatedTrailDrawNode = HitboxesDrawNode::create();
    m_rotatedTrailDrawNode->retain();
    m_rotatedTrailDrawNode->setID("hitbox-trail-node-rotated"_spr);
    m_rotatedTrailDrawNode->setBlendFunc(
        {GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA});

    pl->m_debugDrawNode->getParent()->addChild(
        m_drawNode, pl->m_uiLayer->getZOrder() + 10000);
    pl->m_debugDrawNode->getParent()->addChild(
        m_solidTrailDrawNode, pl->m_uiLayer->getZOrder() + 9998);
    pl->m_debugDrawNode->getParent()->addChild(
        m_rotatedTrailDrawNode, pl->m_uiLayer->getZOrder() + 9996);
    pl->m_debugDrawNode->getParent()->addChild(
        m_innerTrailDrawNode, pl->m_uiLayer->getZOrder() + 9999);
    pl->m_debugDrawNode->getParent()->addChild(
        m_circleTrailDrawNode, pl->m_uiLayer->getZOrder() + 9997);

    m_initialized = true;
}

static void iterateObjects(GJBaseGameLayer* pl,
                           std::function<void(GameObject*)> callback) {
    auto& sections = pl->m_sections;

    for (int i = pl->m_leftSectionIndex;
         i <= pl->m_rightSectionIndex &&
         static_cast<size_t>(i) < sections.size();
         i++) {
        auto horizontalSection = sections[i];
        if (!horizontalSection) continue;

        for (int j = pl->m_bottomSectionIndex;
             j <= pl->m_topSectionIndex &&
             static_cast<size_t>(j) < horizontalSection->size();
             j++) {
            auto section = horizontalSection->at(j);
            if (!section) continue;

            for (int k = 0; k < pl->m_sectionSizes[i]->at(j); k++) {
                auto object = section->at(k);
                if (!object) continue;

                callback(object);
            }
        }
    }
}

const std::array ALWAYS_ALLOWED_MODIFIERS = {200,  201,  202, 203,
                                             1334, 1816, 3643};

struct ResolvedHBCategory {
    bool enabled;
    cocos2d::ccColor4F line;
    cocos2d::ccColor4F fill;
};

static ResolvedHBCategory resolveHB(SLSettings::HitboxSettings& settings,
                                    int type) {
    auto& s = settings.categories[type];
    const float a = s.colors[3];
    return ResolvedHBCategory{
        .enabled = s.enabled,
        .line = {s.colors[0], s.colors[1], s.colors[2], a},
        .fill = {s.colors[0], s.colors[1], s.colors[2],
                 a * static_cast<float>(s.fillOpacity)},
    };
}

struct ResolvedObjectHB {
    float width;
    ResolvedHBCategory interactable;
    ResolvedHBCategory interactableActive;
    ResolvedHBCategory solid;
    ResolvedHBCategory passable;
    ResolvedHBCategory hazard;
};

static void drawObjectHitbox(cocos2d::CCDrawNode* node, GJBaseGameLayer* pl,
                             GameObject* object, const ResolvedObjectHB& hb) {
    const float width = hb.width;

    if (object->m_isGroupDisabled || !object->m_isActivated ||
        object->m_objectType == GameObjectType::Decoration)
        return;

    if ((object == pl->m_player1 || object == pl->m_player2) ||
        Bot::get()->trajectory().isFakePlayer((PlayerObject*)object)) {
        return;
    }

    switch (object->m_objectType) {
        default: {
            if (object->hasBeenActivatedByPlayer(pl->m_player1) &&
                (!pl->m_gameState.m_isDualMode ||
                 object->hasBeenActivatedByPlayer(pl->m_player2))) {
                return;
            }

            if (object == pl->m_player1CollisionBlock ||
                object == pl->m_player2CollisionBlock) {
                return;
            }

            if (object->m_objectType == GameObjectType::Modifier &&
                std::ranges::find(ALWAYS_ALLOWED_MODIFIERS,
                                  object->m_objectID) ==
                    ALWAYS_ALLOWED_MODIFIERS.end()) {
                if (!static_cast<EffectGameObject*>(object)
                         ->m_isTouchTriggered) {
                    return;
                }
            }

            if (!hb.interactable.enabled) break;

            bool isObjectRectDirty = object->m_isObjectRectDirty;
            bool boxOffsetCalculated = object->m_boxOffsetCalculated;

            ResolvedHBCategory const* cat = &hb.interactable;
            if (auto ro = geode::cast::typeinfo_cast<RingObject*>(object); ro) {
                if (pl->m_player1->m_touchingRings->containsObject(object) ||
                    pl->m_player2->m_touchingRings->containsObject(object)) {
                    cat = &hb.interactableActive;
                }
            }

            if (object->getObjectRect().intersectsRect(
                    pl->m_player1->getObjectRect()) ||
                object->getObjectRect().intersectsRect(
                    pl->m_player2->getObjectRect())) {
                cat = &hb.interactableActive;
            }

            if (auto ob = object->m_orientedBox; ob) {
                node->drawPolygon(ob->m_corners.data(), 4, cat->fill, width,
                                  cat->line);
            } else {
                node->drawRect(usingWidth(object->getObjectRect(), width),
                               cat->fill, width, cat->line);
            }

            object->m_isObjectRectDirty = isObjectRectDirty;
            object->m_boxOffsetCalculated = boxOffsetCalculated;
            break;
        }

        case GameObjectType::Solid: {
            const ResolvedHBCategory& cat =
                object->m_isPassable ? hb.passable : hb.solid;
            if (cat.enabled) {
                node->drawRect(usingWidth(object->getObjectRect(), width),
                               cat.fill, width, cat.line);
            }

            break;
        }

        case GameObjectType::Slope: {
            auto rect = object->getObjectRect();
            std::array verts = {
                rect.origin,
                cocos2d::CCPoint{rect.getMinX(), rect.getMaxY()},
                cocos2d::CCPoint{rect.getMaxX(), rect.getMinY()},
            };

            auto max = cocos2d::CCPoint(rect.getMaxX(), rect.getMaxY());
            switch (object->m_slopeDirection) {
                case 0:
                case 7:
                    verts[1] = max;
                    break;
                case 1:
                case 5:
                    verts[0] = max;
                    break;
                case 3:
                case 6:
                    verts[2] = max;
                    break;
            };

            const ResolvedHBCategory& cat =
                object->m_isPassable ? hb.passable : hb.solid;
            if (cat.enabled) {
                node->drawPolygon(verts.data(), 3, cat.fill, width, cat.line);
            }

            break;
        }

        case GameObjectType::AnimatedHazard:
        case GameObjectType::Hazard: {
            if (!object->m_isActivated) return;
            if (object == pl->m_anticheatSpike) return;
            if (!hb.hazard.enabled) return;

            // ripped from updateDebugDraw
            bool isObjectRectDirty = object->m_isObjectRectDirty;
            bool boxOffsetCalculated = object->m_boxOffsetCalculated;

            float radius = object->m_objectRadius *
                           std::max(object->m_scaleX, object->m_scaleY);
            if (radius > 0) {
                int segments = (int)std::max(radius, 8.0f);

                node->drawCircle(object->getPosition(), radius, hb.hazard.fill,
                                 width, hb.hazard.line, segments);
            } else if (auto ob = object->m_orientedBox; ob) {
                node->drawPolygon(ob->m_corners.data(), 4, hb.hazard.fill,
                                  width, hb.hazard.line);
            } else {
                node->drawRect(usingWidth(object->getObjectRect(), width),
                               hb.hazard.fill, width, hb.hazard.line);
            }

            object->m_isObjectRectDirty = isObjectRectDirty;
            object->m_boxOffsetCalculated = boxOffsetCalculated;

            break;
        }
    }
}

void Hitboxes::draw(GJBaseGameLayer* pl) {
    if (!m_initialized) return;

    m_drawNode->clear();
    m_solidTrailDrawNode->clear();
    m_rotatedTrailDrawNode->clear();
    m_innerTrailDrawNode->clear();
    m_circleTrailDrawNode->clear();

    if (!m_enabled->inner()) return;

    if (m_trailEnabled->inner()) {
        float scale = pl->m_objectLayer->getScale();
        float minX =
            -pl->m_objectLayer->getPositionX() / scale - pl->m_cameraWidth;
        float maxX =
            -pl->m_objectLayer->getPositionX() / scale + pl->m_cameraWidth;
        float minY =
            -pl->m_objectLayer->getPositionY() / scale - pl->m_cameraHeight;
        float maxY =
            -pl->m_objectLayer->getPositionY() / scale + pl->m_cameraHeight;
        // auto rect = cocos2d::CCRect(
        //     minX, minY,
        //     maxX - minX, maxY - minY
        // );

        // m_innerTrailDrawNode->enableDrawArea(rect);
        // m_rotatedTrailDrawNode->enableDrawArea(rect);
        // m_solidTrailDrawNode->enableDrawArea(rect);

        auto& settings = SLSettings::get()->hitboxes;
        using Type = SLSettings::HitboxSettings::Type;

        const float width = settings.width / pl->m_gameState.m_cameraZoom;

        const bool eRotated = HB_ENABLED(Type::PlayerRotated);
        const bool eSolid = HB_ENABLED(Type::Player);
        const bool eCircle = HB_ENABLED(Type::PlayerCircle);
        const bool eInner = HB_ENABLED(Type::PlayerInner);

        float* rotatedColors =
            settings.categories[Type::PlayerRotated].colors.data();
        float rotatedFill =
            settings.categories[Type::PlayerRotated].fillOpacity;
        float* solidColors = settings.categories[Type::Player].colors.data();
        float solidFill = settings.categories[Type::Player].fillOpacity;
        float* circleColors =
            settings.categories[Type::PlayerCircle].colors.data();
        float circleFill = settings.categories[Type::PlayerCircle].fillOpacity;
        float* innerColors =
            settings.categories[Type::PlayerInner].colors.data();
        float innerFill = settings.categories[Type::PlayerInner].fillOpacity;

        const float objX = pl->m_objectLayer->getPositionX();
        const float objY = pl->m_objectLayer->getPositionY();

        auto drawTrail = [&](std::deque<HitboxTrailUnit>& trail) {
            bool hasLast = false;
            int lastPx = 0;
            int lastPy = 0;
            for (auto& unit : trail) {
                if (!unit.shouldDraw(minX, maxX, minY, maxY)) continue;

                // Collapse consecutive segments that map to the same on-screen
                // pixel; keeps the trail as dense as the current zoom allows
                // without overdrawing identical positions.
                const float SUBPIXEL_FACTOR = 3;
                const int px = static_cast<int>(std::floor(
                    (unit.m_rect.getMidX() * scale + objX) * SUBPIXEL_FACTOR));
                const int py = static_cast<int>(std::floor(
                    (unit.m_rect.getMidY() * scale + objY) * SUBPIXEL_FACTOR));
                if (hasLast && px == lastPx && py == lastPy) continue;
                hasLast = true;
                lastPx = px;
                lastPy = py;

                if (eRotated)
                    unit.drawRotated(m_rotatedTrailDrawNode, width,
                                     rotatedColors, rotatedFill);
                if (eSolid)
                    unit.draw(m_solidTrailDrawNode, width, solidColors,
                              solidFill);
                if (eCircle)
                    unit.drawCircle(m_circleTrailDrawNode, width, circleColors,
                                    circleFill);
                if (eInner)
                    unit.drawInner(m_innerTrailDrawNode, width, innerColors,
                                   innerFill);
            }
        };

        if (eRotated || eSolid || eCircle || eInner) {
            drawTrail(m_trailP1);
            drawTrail(m_trailP2);
        }
    }

    {
        using Type = SLSettings::HitboxSettings::Type;
        auto& hbs = SLSettings::get()->hitboxes;
        const ResolvedObjectHB objHB{
            .width =
                static_cast<float>(hbs.width / pl->m_gameState.m_cameraZoom),
            .interactable = resolveHB(hbs, Type::Interactable),
            .interactableActive = resolveHB(hbs, Type::InteractableActive),
            .solid = resolveHB(hbs, Type::Solid),
            .passable = resolveHB(hbs, Type::Passable),
            .hazard = resolveHB(hbs, Type::Hazard),
        };

        iterateObjects(pl, [&](GameObject* object) {
            drawObjectHitbox(this->m_drawNode, pl, object, objHB);
        });
    }

    drawPlayerHitbox(this->m_drawNode, pl, pl->m_player1,
                     SLSettings::get()->hitboxes);
    if (pl->m_gameState.m_isDualMode) {
        drawPlayerHitbox(this->m_drawNode, pl, pl->m_player2,
                         SLSettings::get()->hitboxes);
    }
}

static constexpr size_t TRAIL_MAX_UNITS = 69420;

static void appendTrailUnit(std::deque<HitboxTrailUnit>& trail,
                            PlayerObject* player, const cocos2d::CCRect& rect) {
    auto* box = player->getOrientedBox();
    trail.push_back({
        .m_rect = rect,
        .m_scaled = player->getObjectRect(0.3, 0.3),
        .m_rotated = box ? box->m_corners : std::array<cocos2d::CCPoint, 4>{},
    });

    if (trail.size() > TRAIL_MAX_UNITS) {
        trail.pop_front();
    }
}

void Hitboxes::saveToTrail(GJBaseGameLayer* pl) {
    if (pl->m_levelEndAnimationStarted) {
        return;  // don't
    }

    if (!m_enabled->inner() || !m_trailEnabled->inner()) {
        return;
    }

    appendTrailUnit(m_trailP1, pl->m_player1, pl->m_player1->getObjectRect());
    appendTrailUnit(m_trailP2, pl->m_player2, pl->m_player2->getObjectRect());
}

void Hitboxes::clearTrail() {
    m_trailP1.clear();
    m_trailP2.clear();
}

void Hitboxes::destroy() {
    if (!m_initialized) return;

    CC_SAFE_RELEASE(m_drawNode);
    m_initialized = false;
}
