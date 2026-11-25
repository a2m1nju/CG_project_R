#include "CharacterModels.h"
#include <cmath>

void drawPart(GLuint shader, glm::mat4 parentModel, glm::vec3 offset,
              glm::vec3 scale, glm::vec3 color);
void drawChicken(GLuint shader, glm::mat4 baseModel);

static void drawTino(GLuint shader, const glm::mat4& model) {
    const glm::vec3 white(0.98f, 0.98f, 0.96f);
    const glm::vec3 blue(0.03f, 0.35f, 0.68f);
    const glm::vec3 dark(0.09f, 0.12f, 0.17f);

    drawPart(shader, model, {0.0f, -0.05f, 0.0f}, {0.78f, 0.78f, 0.68f}, white);
    drawPart(shader, model, {0.0f, 0.39f, -0.02f}, {0.66f, 0.55f, 0.58f}, white);
    drawPart(shader, model, {0.0f, -0.07f, -0.35f}, {0.42f, 0.49f, 0.06f}, blue);
    for (float x : {-0.20f, 0.20f}) {
        drawPart(shader, model, {x, -0.47f, -0.02f}, {0.18f, 0.25f, 0.25f}, white);
        drawPart(shader, model, {x * 1.85f, -0.09f, -0.10f}, {0.16f, 0.36f, 0.22f}, white);
        drawPart(shader, model, {x * 0.85f, 0.45f, -0.33f}, {0.055f, 0.06f, 0.04f}, dark);
        drawPart(shader, model, {x * 1.35f, 0.29f, -0.33f}, {0.10f, 0.10f, 0.04f}, blue);
    }
    drawPart(shader, model, {0.0f, 0.68f, 0.14f}, {0.15f, 0.17f, 0.16f}, blue);
    drawPart(shader, model, {0.0f, 0.43f, 0.24f}, {0.19f, 0.20f, 0.17f}, blue);
    drawPart(shader, model, {0.0f, 0.16f, 0.35f}, {0.20f, 0.21f, 0.16f}, blue);
    drawPart(shader, model, {0.0f, -0.30f, 0.44f}, {0.30f, 0.23f, 0.45f}, white);
    drawPart(shader, model, {0.0f, -0.38f, 0.72f}, {0.22f, 0.15f, 0.32f}, white);
    drawPart(shader, model, {0.0f, 0.26f, -0.33f}, {0.18f, 0.025f, 0.025f}, dark);
}

static void drawMonokuma(GLuint shader, const glm::mat4& model) {
    const glm::vec3 white(0.96f, 0.96f, 0.94f);
    const glm::vec3 black(0.06f, 0.08f, 0.10f);
    const glm::vec3 red(0.95f, 0.08f, 0.14f);

    for (float side : {-1.0f, 1.0f}) {
        const glm::vec3 fur = side < 0.0f ? white : black;
        drawPart(shader, model, {side * 0.19f, -0.12f, 0.0f},
                 {0.39f, 0.70f, 0.58f}, fur);
        drawPart(shader, model, {side * 0.18f, 0.35f, 0.0f},
                 {0.37f, 0.51f, 0.56f}, fur);
        drawPart(shader, model, {side * 0.24f, 0.67f, -0.05f},
                 {0.19f, 0.19f, 0.19f}, fur);
        drawPart(shader, model, {side * 0.19f, -0.51f, -0.03f},
                 {0.21f, 0.20f, 0.26f}, fur);
        drawPart(shader, model, {side * 0.38f, -0.11f, -0.10f},
                 {0.19f, 0.20f, 0.24f}, fur);
    }
    drawPart(shader, model, {0.10f, -0.05f, -0.34f},
             {0.42f, 0.14f, 0.14f}, black);
    drawPart(shader, model, {-0.10f, -0.12f, -0.39f},
             {0.42f, 0.14f, 0.14f}, white);

    drawPart(shader, model, {-0.15f, 0.39f, -0.34f},
             {0.06f, 0.06f, 0.03f}, black);
    drawPart(shader, model, {0.15f, 0.40f, -0.34f},
             {0.17f, 0.07f, 0.03f}, red);
    drawPart(shader, model, {0.15f, 0.43f, -0.36f},
             {0.08f, 0.08f, 0.02f}, red);
    drawPart(shader, model, {0.0f, 0.27f, -0.34f},
             {0.18f, 0.12f, 0.08f}, white);
    drawPart(shader, model, {0.0f, 0.29f, -0.39f},
             {0.07f, 0.05f, 0.03f}, black);
    drawPart(shader, model, {0.15f, 0.20f, -0.33f},
             {0.24f, 0.09f, 0.03f}, white);
    for (float x : {0.07f, 0.15f, 0.23f})
        drawPart(shader, model, {x, 0.20f, -0.35f},
                 {0.015f, 0.08f, 0.01f}, black);
    drawPart(shader, model, {-0.14f, -0.10f, -0.22f},
             {0.24f, 0.33f, 0.04f}, white);
    drawPart(shader, model, {0.14f, -0.10f, -0.22f},
             {0.24f, 0.33f, 0.04f}, black);
    drawPart(shader, model, {-0.10f, -0.33f, 0.36f},
             {0.20f, 0.18f, 0.28f}, white);
    drawPart(shader, model, {0.10f, -0.33f, 0.36f},
             {0.20f, 0.18f, 0.28f}, black);
}

static void drawKorilakkuma(GLuint shader, const glm::mat4& model) {
    const glm::vec3 cream(0.88f, 0.76f, 0.57f);
    const glm::vec3 white(0.98f, 0.96f, 0.94f);
    const glm::vec3 pink(0.95f, 0.53f, 0.67f);
    const glm::vec3 brown(0.22f, 0.12f, 0.10f);
    const glm::vec3 red(0.88f, 0.08f, 0.10f);

    drawPart(shader, model, {0.0f, -0.17f, 0.0f}, {0.69f, 0.63f, 0.60f}, cream);
    drawPart(shader, model, {0.0f, 0.37f, -0.04f}, {0.80f, 0.59f, 0.58f}, cream);
    drawPart(shader, model, {0.0f, -0.27f, -0.31f}, {0.38f, 0.36f, 0.04f}, white);
    for (float x : {-0.26f, 0.26f}) {
        drawPart(shader, model, {x, 0.69f, -0.12f}, {0.24f, 0.24f, 0.20f}, cream);
        drawPart(shader, model, {x, 0.70f, -0.23f}, {0.17f, 0.17f, 0.025f}, pink);
        drawPart(shader, model, {x * 1.38f, -0.12f, -0.09f},
                 {0.18f, 0.36f, 0.23f}, cream);
        drawPart(shader, model, {x * 0.77f, -0.54f, 0.02f},
                 {0.20f, 0.24f, 0.26f}, cream);
        drawPart(shader, model, {x * 0.80f, 0.38f, -0.35f},
                 {0.09f, 0.09f, 0.035f}, brown);
    }
    drawPart(shader, model, {0.0f, 0.23f, -0.35f}, {0.26f, 0.18f, 0.06f}, white);
    drawPart(shader, model, {0.0f, 0.29f, -0.40f}, {0.06f, 0.06f, 0.03f}, brown);
    drawPart(shader, model, {0.0f, 0.21f, -0.40f}, {0.025f, 0.08f, 0.025f}, brown);
    drawPart(shader, model, {0.0f, 0.17f, -0.40f},
             {0.21f, 0.025f, 0.025f}, brown);
    drawPart(shader, model, {0.0f, -0.06f, -0.32f}, {0.14f, 0.14f, 0.04f}, red);
    for (float x : {-0.035f, 0.035f})
        for (float y : {-0.10f, -0.02f})
            drawPart(shader, model, {x, y, -0.35f},
                     {0.018f, 0.018f, 0.01f}, white);
}

static void drawKuromi(GLuint shader, const glm::mat4& model) {
    const glm::vec3 black(0.13f, 0.12f, 0.14f);
    const glm::vec3 white(0.98f, 0.97f, 0.96f);
    const glm::vec3 pink(0.94f, 0.55f, 0.69f);

    drawPart(shader, model, {0.0f, -0.21f, -0.04f}, {0.61f, 0.62f, 0.54f}, white);
    drawPart(shader, model, {0.0f, 0.33f, -0.05f}, {0.80f, 0.66f, 0.55f}, black);
    drawPart(shader, model, {0.0f, 0.19f, -0.37f}, {0.61f, 0.31f, 0.06f}, white);
    drawPart(shader, model, {0.0f, -0.01f, -0.14f}, {0.53f, 0.17f, 0.44f}, black);

    for (float side : {-1.0f, 1.0f}) {
        glm::mat4 horn = glm::translate(model, {side * 0.32f, 0.69f, -0.08f});
        horn = glm::rotate(horn, glm::radians(-side * 25.0f),
                           glm::vec3(0.0f, 0.0f, 1.0f));
        drawPart(shader, horn, {0.0f, 0.18f, 0.0f}, {0.21f, 0.49f, 0.20f}, black);
        drawPart(shader, model, {side * 0.51f, 1.06f, -0.08f},
                 {0.13f, 0.13f, 0.13f}, black);
        drawPart(shader, model, {side * 0.32f, -0.23f, -0.12f},
                 {0.16f, 0.30f, 0.18f}, white);
        drawPart(shader, model, {side * 0.18f, -0.54f, -0.02f},
                 {0.19f, 0.24f, 0.22f}, white);
        drawPart(shader, model, {side * 0.38f, -0.20f, -0.21f},
                 {0.08f, 0.08f, 0.04f}, pink);
        drawPart(shader, model, {side * 0.18f, 0.21f, -0.42f},
                 {0.09f, 0.13f, 0.03f}, black);
    }
    drawPart(shader, model, {0.0f, 0.46f, -0.37f}, {0.21f, 0.17f, 0.04f}, pink);
    for (float x : {-0.07f, 0.07f})
        drawPart(shader, model, {x, 0.47f, -0.40f},
                 {0.035f, 0.045f, 0.02f}, black);
    drawPart(shader, model, {0.0f, 0.39f, -0.40f}, {0.09f, 0.025f, 0.02f}, black);
    drawPart(shader, model, {0.0f, 0.13f, -0.43f}, {0.045f, 0.04f, 0.02f}, pink);
    drawPart(shader, model, {0.0f, 0.06f, -0.43f}, {0.11f, 0.025f, 0.02f}, black);
    drawPart(shader, model, {0.0f, -0.37f, 0.30f}, {0.14f, 0.12f, 0.30f}, black);
    drawPart(shader, model, {0.0f, -0.29f, 0.50f}, {0.18f, 0.10f, 0.18f}, black);
}

static void drawSans(GLuint shader, const glm::mat4& model) {
    const glm::vec3 bone(0.97f, 0.97f, 0.93f);
    const glm::vec3 black(0.06f, 0.07f, 0.09f);
    const glm::vec3 blue(0.08f, 0.55f, 0.91f);
    const glm::vec3 darkBlue(0.04f, 0.32f, 0.58f);

    // 재킷, 셔츠, 반바지와 슬리퍼.
    drawPart(shader, model, {0.0f, -0.17f, 0.02f}, {0.60f, 0.53f, 0.40f}, blue);
    drawPart(shader, model, {0.0f, -0.18f, -0.20f}, {0.28f, 0.44f, 0.04f}, bone);
    for (float side : {-1.0f, 1.0f}) {
        drawPart(shader, model, {side * 0.22f, -0.15f, -0.20f},
                 {0.25f, 0.52f, 0.08f}, blue);
        drawPart(shader, model, {side * 0.43f, -0.19f, 0.0f},
                 {0.27f, 0.42f, 0.33f}, blue);
        drawPart(shader, model, {side * 0.48f, -0.39f, -0.03f},
                 {0.20f, 0.16f, 0.24f}, bone);
        drawPart(shader, model, {side * 0.15f, -0.48f, 0.01f},
                 {0.29f, 0.26f, 0.38f}, black);
        drawPart(shader, model, {side * 0.17f, -0.63f, -0.06f},
                 {0.16f, 0.13f, 0.22f}, bone);
        drawPart(shader, model, {side * 0.20f, -0.70f, -0.15f},
                 {0.31f, 0.09f, 0.33f}, bone);
        drawPart(shader, model, {side * 0.24f, -0.41f, -0.22f},
                 {0.16f, 0.07f, 0.04f}, darkBlue);
    }

    // 넓은 두개골, 눈구멍, 코와 이가 드러난 미소.
    drawPart(shader, model, {0.0f, 0.39f, -0.04f}, {0.78f, 0.59f, 0.58f}, bone);
    drawPart(shader, model, {0.0f, 0.14f, -0.10f}, {0.65f, 0.25f, 0.46f}, bone);
    for (float side : {-1.0f, 1.0f}) {
        drawPart(shader, model, {side * 0.19f, 0.42f, -0.35f},
                 {0.19f, 0.16f, 0.05f}, black);
        drawPart(shader, model, {side * 0.265f, 0.15f, -0.37f},
                 {0.08f, 0.075f, 0.05f}, black);
    }
    drawPart(shader, model, {0.0f, 0.29f, -0.36f}, {0.08f, 0.07f, 0.05f}, black);
    drawPart(shader, model, {0.0f, 0.13f, -0.37f}, {0.52f, 0.10f, 0.05f}, black);
    for (float x : {-0.15f, -0.05f, 0.05f, 0.15f})
        drawPart(shader, model, {x, 0.12f, -0.41f},
                 {0.06f, 0.055f, 0.02f}, bone);
}

static void drawAccessories(GLuint shader, const glm::mat4& model,
                            CharacterId id, HeadAccessory head, BackAccessory back,
                            FaceAccessory face) {
    const bool fourLegs = id >= CharacterId::Rabbit && id <= CharacterId::Panda;
    const float headY = fourLegs ? 0.52f : id == CharacterId::Chicken ? 0.42f : 0.77f;
    const float headZ = fourLegs ? -0.20f : -0.08f;
    if (CanEquipHead(id, head)) {
        if (head == HeadAccessory::Ribbon) {
            const glm::vec3 red(0.88f, 0.16f, 0.27f);
            drawPart(shader, model, {-0.33f, headY, headZ - 0.08f},
                     {0.16f, 0.11f, 0.10f}, red);
            drawPart(shader, model, {-0.10f, headY, headZ - 0.08f},
                     {0.16f, 0.11f, 0.10f}, red);
            drawPart(shader, model, {-0.22f, headY, headZ - 0.11f},
                     {0.08f, 0.12f, 0.12f}, glm::vec3(1.0f, 0.37f, 0.47f));
        } else if (head == HeadAccessory::Crown) {
            const glm::vec3 gold(1.0f, 0.78f, 0.12f);
            drawPart(shader, model, {0.0f, headY + 0.09f, headZ},
                     {0.37f, 0.13f, 0.30f}, gold);
            for (float x : {-0.14f, 0.0f, 0.14f})
                drawPart(shader, model, {x, headY + 0.20f, headZ},
                         {0.08f, 0.15f, 0.09f}, gold);
        } else if (head == HeadAccessory::Hat) {
            const glm::vec3 navy(0.13f, 0.24f, 0.44f);
            const float hatY = id == CharacterId::Chicken ? 0.33f : 0.35f;
            drawPart(shader, model, {0.0f, hatY, headZ},
                     {0.64f, 0.07f, 0.52f}, navy);
            drawPart(shader, model, {0.0f, hatY + 0.13f, headZ + 0.04f},
                     {0.43f, 0.23f, 0.40f}, navy);
            drawPart(shader, model, {0.0f, hatY + 0.06f, headZ - 0.17f},
                     {0.44f, 0.05f, 0.05f}, glm::vec3(0.95f, 0.73f, 0.24f));
        } else if (head == HeadAccessory::Halo) {
            const glm::vec3 gold(1.0f, 0.85f, 0.27f);
            // 머리 위에 고리를 수평으로 놓고 조각을 겹쳐 끊김 없이 이어 준다.
            const float ringY = id == CharacterId::Chicken ? 0.43f :
                id == CharacterId::Rabbit ? 0.88f :
                id == CharacterId::Cat ? 0.65f :
                id == CharacterId::Dog ? 0.55f :
                id == CharacterId::Bear || id == CharacterId::Panda ? 0.63f :
                id == CharacterId::Tino ? 0.88f :
                id == CharacterId::Monokuma ? 0.91f :
                id == CharacterId::Korilakkuma ? 0.96f :
                id == CharacterId::Sans ? 0.81f : 1.28f;
            const float ringZ = fourLegs ? -0.20f : -0.04f;
            for (int i = 0; i < 24; ++i) {
                const float angle = glm::radians(15.0f * i);
                glm::mat4 segment = glm::translate(model,
                    {0.29f * std::cos(angle), ringY,
                     ringZ + 0.23f * std::sin(angle)});
                segment = glm::rotate(segment,
                    std::atan2(-0.23f * std::cos(angle),
                               -0.29f * std::sin(angle)),
                    glm::vec3(0.0f, 1.0f, 0.0f));
                drawPart(shader, segment, {0.0f, 0.0f, 0.0f},
                         {0.12f, 0.08f, 0.08f}, gold);
            }
        } else if (head == HeadAccessory::Horns) {
            const glm::vec3 red(0.67f, 0.09f, 0.15f);
            const float baseY = id == CharacterId::Chicken ? 0.32f :
                id == CharacterId::Dog ? 0.45f : 0.78f;
            const float hornZ = id == CharacterId::Chicken ? -0.18f :
                id == CharacterId::Dog ? -0.30f : -0.27f;
            for (float side : {-1.0f, 1.0f}) {
                drawPart(shader, model, {side * 0.23f, baseY, hornZ},
                         {0.16f, 0.24f, 0.16f}, red);
                drawPart(shader, model, {side * 0.29f, baseY + 0.18f, hornZ},
                         {0.10f, 0.17f, 0.10f}, red);
            }
        } else if (head == HeadAccessory::Flower) {
            const glm::vec3 pink(0.98f, 0.48f, 0.66f);
            const float x = -0.31f;
            const float flowerY = id == CharacterId::Kuromi ? 0.63f : headY + 0.10f;
            const float flowerZ = id == CharacterId::Kuromi ? -0.40f : headZ - 0.21f;
            for (const glm::vec3 offset : {
                     glm::vec3(-0.10f, 0.0f, 0.0f), glm::vec3(0.10f, 0.0f, 0.0f),
                     glm::vec3(0.0f, -0.10f, 0.0f), glm::vec3(0.0f, 0.10f, 0.0f)})
                drawPart(shader, model, {x + offset.x, flowerY + offset.y,
                         flowerZ}, {0.14f, 0.14f, 0.07f}, pink);
            drawPart(shader, model, {x, flowerY, flowerZ - 0.05f},
                     {0.11f, 0.11f, 0.08f}, glm::vec3(1.0f, 0.82f, 0.20f));
        }
    }
    if (CanEquipFace(id, face) && face != FaceAccessory::None) {
        const float eyeY = id == CharacterId::Kuromi ? 0.21f :
            fourLegs ? 0.10f : id == CharacterId::Chicken ? 0.0f : 0.39f;
        const float eyeZ = fourLegs ? -0.57f : id == CharacterId::Chicken ? -0.38f : -0.43f;
        const glm::vec3 frame = face == FaceAccessory::Glasses ?
            glm::vec3(0.16f, 0.13f, 0.12f) : glm::vec3(0.08f, 0.08f, 0.10f);
        for (float side : {-1.0f, 1.0f}) {
            const float x = side * 0.18f;
            if (face == FaceAccessory::Sunglasses)
                drawPart(shader, model, {x, eyeY, eyeZ - 0.025f},
                         {0.27f, 0.16f, 0.035f}, glm::vec3(0.04f, 0.06f, 0.10f));
            else {
                for (float edge : {-1.0f, 1.0f}) {
                    drawPart(shader, model, {x, eyeY + edge * 0.09f, eyeZ},
                             {0.29f, 0.025f, 0.04f}, frame);
                    drawPart(shader, model, {x + edge * 0.14f, eyeY, eyeZ},
                             {0.025f, 0.19f, 0.04f}, frame);
                }
            }
        }
        drawPart(shader, model, {0.0f, eyeY, eyeZ}, {0.10f, 0.03f, 0.04f}, frame);
    }
    if (CanEquipBack(id, back) && back == BackAccessory::Backpack) {
        const glm::vec3 blue(0.15f, 0.43f, 0.83f);
        const glm::vec3 yellow(0.97f, 0.74f, 0.23f);
        const glm::vec3 pos = fourLegs ? glm::vec3(0.0f, 0.27f, 0.17f) :
            id == CharacterId::Chicken ? glm::vec3(0.0f, -0.04f, 0.44f) :
            glm::vec3(0.0f, -0.10f, 0.43f);
        drawPart(shader, model, pos, {0.36f, 0.29f, 0.24f}, blue);
        drawPart(shader, model, pos + glm::vec3(0.0f, 0.12f, 0.03f),
                 {0.38f, 0.08f, 0.26f}, yellow);
    } else if (CanEquipBack(id, back) && back != BackAccessory::None) {
        const bool angel = back == BackAccessory::AngelWings;
        const glm::vec3 color = angel ? glm::vec3(0.96f, 0.97f, 1.0f) :
            glm::vec3(0.35f, 0.08f, 0.20f);
        const glm::vec3 edge = angel ? glm::vec3(0.75f, 0.85f, 0.98f) :
            glm::vec3(0.10f, 0.07f, 0.14f);
        const float z = fourLegs ? 0.40f : 0.48f;
        const float y = fourLegs ? 0.05f : -0.06f;
        const float flap = std::sin(
            static_cast<float>(glutGet(GLUT_ELAPSED_TIME)) * 0.006f) * 22.0f;
        for (float side : {-1.0f, 1.0f}) {
            glm::mat4 wing = glm::translate(model, {side * 0.37f, y, z});
            wing = glm::rotate(wing, glm::radians(side * (20.0f + flap)),
                               glm::vec3(0.0f, 0.0f, 1.0f));
            drawPart(shader, wing, {side * 0.20f, 0.04f, 0.0f},
                     {0.42f, 0.42f, 0.11f}, color);
            drawPart(shader, wing, {side * 0.43f, angel ? -0.13f : 0.18f, 0.0f},
                     {0.22f, 0.42f, 0.10f}, color);
            drawPart(shader, wing, {side * 0.28f, -0.25f, 0.02f},
                     {0.18f, 0.30f, 0.10f}, edge);
        }
    }
}

static void drawCharacterBody(GLuint shader, const glm::mat4& model,
                              CharacterId id, HeadAccessory head) {
    if (id == CharacterId::Chicken) {
        drawChicken(shader, model);
        return;
    }
    if (id == CharacterId::Tino) {
        drawTino(shader, model);
        return;
    }
    if (id == CharacterId::Monokuma) {
        drawMonokuma(shader, model);
        return;
    }
    if (id == CharacterId::Korilakkuma) {
        drawKorilakkuma(shader, model);
        return;
    }
    if (id == CharacterId::Kuromi) {
        drawKuromi(shader, model);
        return;
    }
    if (id == CharacterId::Sans) {
        drawSans(shader, model);
        return;
    }

    const glm::vec3 dark(0.12f, 0.10f, 0.10f);
    const glm::vec3 pink(0.95f, 0.48f, 0.53f);
    const glm::vec3 white(0.96f, 0.95f, 0.88f);
    const glm::vec3 cream(0.82f, 0.79f, 0.70f);
    const glm::vec3 brown(0.58f, 0.37f, 0.22f);
    const glm::vec3 orange(0.93f, 0.55f, 0.24f);
    const bool rabbit = id == CharacterId::Rabbit;
    const bool cat = id == CharacterId::Cat;
    const bool dog = id == CharacterId::Dog;
    const bool panda = id == CharacterId::Panda;
    const glm::vec3 fur = rabbit || panda || id == CharacterId::Bear ? white :
        cat ? orange : brown;
    const glm::vec3 legs = panda ? dark : fur;

    // 얼굴과 몸통의 앞쪽을 맞추고 몸통은 +Z 방향으로 뻗게 한다.
    drawPart(shader, model, {0.0f, -0.12f, 0.05f}, {0.72f, 0.58f, 1.10f}, fur);
    drawPart(shader, model, {0.0f, 0.04f, -0.20f}, {0.67f, 0.58f, 0.60f}, fur);
    for (float x : {-0.25f, 0.25f}) {
        for (float z : {-0.20f, 0.45f}) {
            drawPart(shader, model, {x, -0.42f, z}, {0.18f, 0.35f, 0.21f}, legs);
            drawPart(shader, model, {x, -0.60f, z - 0.04f},
                     {0.20f, 0.08f, 0.24f}, panda ? dark : rabbit ? white : cream);
        }
    }

    if (rabbit) {
        for (float x : {-0.18f, 0.18f}) {
            drawPart(shader, model, {x, 0.48f, -0.20f}, {0.13f, 0.55f, 0.15f}, fur);
            drawPart(shader, model, {x, 0.48f, -0.29f}, {0.07f, 0.38f, 0.02f}, pink);
        }
        drawPart(shader, model, {0.0f, -0.17f, 0.65f}, {0.23f, 0.18f, 0.23f}, fur);
    } else if (cat) {
        for (float x : {-0.22f, 0.22f}) {
            drawPart(shader, model, {x, 0.36f, -0.19f}, {0.17f, 0.24f, 0.15f}, fur);
            drawPart(shader, model, {x, 0.36f, -0.28f}, {0.08f, 0.13f, 0.02f}, pink);
        }
        drawPart(shader, model, {0.27f, -0.13f, 0.70f}, {0.10f, 0.12f, 0.46f}, fur);
    } else if (dog) {
        for (float x : {-0.32f, 0.32f})
            drawPart(shader, model, {x, 0.09f, -0.18f}, {0.15f, 0.34f, 0.20f}, dark);
        drawPart(shader, model, {0.0f, -0.09f, 0.68f}, {0.14f, 0.13f, 0.36f}, fur);
    } else {
        if (head != HeadAccessory::Hat) {
            for (float x : {-0.23f, 0.23f})
                drawPart(shader, model, {x, 0.37f, -0.19f},
                         {0.17f, 0.17f, 0.17f}, panda ? dark : fur);
        }
        if (panda) {
            drawPart(shader, model, {0.0f, -0.10f, -0.03f},
                     {0.59f, 0.49f, 0.20f}, dark);
            for (float x : {-0.17f, 0.17f})
                drawPart(shader, model, {x, 0.08f, -0.52f},
                         {0.17f, 0.17f, 0.04f}, dark);
        }
    }

    if (dog || id == CharacterId::Bear || panda)
        drawPart(shader, model, {0.0f, -0.11f, -0.54f},
                 {0.26f, 0.17f, 0.18f}, dog ? cream : white);
    for (float x : {-0.17f, 0.17f})
        drawPart(shader, model, {x, 0.10f, -0.52f},
                 {0.05f, 0.05f, 0.04f}, dark);
    drawPart(shader, model, {0.0f, -0.13f, rabbit || cat ? -0.51f : -0.65f},
             {0.10f, 0.08f, 0.06f}, rabbit || cat ? pink : dark);
}

void drawCharacter(GLuint shader, const glm::mat4& model, CharacterId id,
                   HeadAccessory head, BackAccessory back, FaceAccessory face) {
    id = NormalizeCharacterId(static_cast<std::uint8_t>(id));
    drawCharacterBody(shader, model, id, head);
    drawAccessories(shader, model, id, head, back, face);
}
