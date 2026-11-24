#include "ResultUI.h"
#include "Global.h"
#include "Renderer.h"
#include "ClientNetwork.h"

#include <array>

namespace {

    constexpr float W = 1280.0f;
    constexpr float H = 720.0f;

    enum class Page {
        Podium,
        Stats
    };

    bool active = false;

    Page page = Page::Podium;

    struct Entry {
        std::uint32_t playerId;

        int slot;           // 화면 표시 번호: 1P, 2P, 3P
        int rank;           // 이미 결정된 순위
        int distance;
        int seconds;
        int used;

        int appearanceId;   // 나중에 커스터마이징 연결
    };

    // 1일차 임시 데이터.
    // 실제 결과 패킷을 연결할 때 이 데이터 공급 부분을 바꾸면 됨.
    const std::array<Entry, 3> entries = { {
        {3, 3, 1, 92, 52, 2, 0},
        {1, 1, 2, 80, 45, 3, 0},
        {2, 2, 3, 65, 40, 6, 0}
    } };

    constexpr std::uint32_t previewMyId = 1;

    struct Rect {
        float x;
        float y;
        float w;
        float h;
    };

    const Rect nextButton{ 520, 628, 240, 62 };
    const Rect backButton{ 130, 628, 240, 62 };
    const Rect okButton{ 910, 628, 240, 62 };

    struct Viewport {
        int x;
        int y;
        int w;
        int h;
    };

    // 창 크기가 달라도 16:9 화면 비율 유지.
    Viewport FitViewport()
    {
        const int ww = (std::max)(1, glutGet(GLUT_WINDOW_WIDTH));
        const int wh = (std::max)(1, glutGet(GLUT_WINDOW_HEIGHT));
        const float s = (std::min)(ww / W, wh / H);
        const int vw = (std::max)(1, static_cast<int>(W * s));
        const int vh = (std::max)(1, static_cast<int>(H * s));

        return {
            (ww - vw) / 2,
            (wh - vh) / 2,
            vw,
            vh
        };
    }

    void Begin2D()
    {
        glUseProgram(0);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);

        glEnable(GL_BLEND);
        glBlendFunc(
            GL_SRC_ALPHA,
            GL_ONE_MINUS_SRC_ALPHA
        );

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        glOrtho(0, W, H, 0, -1, 1);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }

    void Box(
        const Rect& r,
        float red,
        float green,
        float blue
    )
    {
        glDisable(GL_TEXTURE_2D);
        glColor3f(red, green, blue);

        glBegin(GL_QUADS);

        glVertex2f(r.x, r.y);
        glVertex2f(r.x + r.w, r.y);
        glVertex2f(r.x + r.w, r.y + r.h);
        glVertex2f(r.x, r.y + r.h);

        glEnd();
    }

    float TextWidth(const std::string& text, float scale)
    {
        float width = 0;

        for (unsigned char c : text) {
            if (c >= 32 && c < 128) {
                width +=
                    cdata[c - 32].xadvance * scale;
            }
        }

        return width;
    }

    // cx: 글자 가로 중앙.
    // baseline: 글자 기준선.
    // 좌표는 결과 화면 전용 1280 x 720 기준.
    void Text(
        float cx,
        float baseline,
        const std::string& text,
        float scale = .55f,
        float maxWidth = 1100
    )
    {
        const float width = TextWidth(text, scale);

        if (width > maxWidth) {
            scale *= maxWidth / width;
        }

        const float start = cx - TextWidth(text, scale) * .5f;

        glActiveTexture(GL_TEXTURE0);

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, fontTexture);

        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

        glColor3f(1, 1, 1);

        float penX = 0;
        float penY = 0;

        glBegin(GL_QUADS);

        for (unsigned char c : text) {
            if (c < 32 || c >= 128) {
                continue;
            }

            stbtt_aligned_quad q;

            // 기존 initFont()의 폰트 이미지 크기는 512 x 512.
            stbtt_GetBakedQuad(
                cdata,
                512,
                512,
                c - 32,
                &penX,
                &penY,
                &q,
                1
            );

            glTexCoord2f(q.s0, q.t0);
            glVertex2f(
                start + q.x0 * scale,
                baseline + q.y0 * scale
            );

            glTexCoord2f(q.s1, q.t0);
            glVertex2f(
                start + q.x1 * scale,
                baseline + q.y0 * scale
            );

            glTexCoord2f(q.s1, q.t1);
            glVertex2f(
                start + q.x1 * scale,
                baseline + q.y1 * scale
            );

            glTexCoord2f(q.s0, q.t1);
            glVertex2f(
                start + q.x0 * scale,
                baseline + q.y1 * scale
            );
        }

        glEnd();

        glDisable(GL_TEXTURE_2D);
    }

    void Button(
        const Rect& r,
        const char* label
    )
    {
        Box(r, .27f, .40f, .49f);

        Box(
            {
                r.x + 3,
                r.y + 3,
                r.w - 6,
                r.h - 6
            },
            .12f,
            .23f,
            .30f
        );

        Text(
            r.x + r.w * .5f,
            r.y + 42,
            label,
            .52f,
            r.w - 24
        );
    }

    bool Contains(const Rect& r, float x, float y)
    {
        return
            x >= r.x &&
            x <= r.x + r.w &&
            y >= r.y &&
            y <= r.y + r.h;
    }

    std::string PlayerName(const Entry& e)
    {
        return
            (e.playerId == previewMyId ? "YOU: " : "") +
            std::to_string(e.slot) +
            "P";
    }

    const char* RankName(int rank)
    {
        return
            rank == 1 ? "1st" :
            rank == 2 ? "2nd" :
            "3rd";
    }

    glm::mat4 PodiumView()
    {
        return glm::lookAt(
            glm::vec3(0, 5.5f, 12),
            glm::vec3(0, 2, 0),
            glm::vec3(0, 1, 0)
        );
    }

    glm::mat4 PodiumProjection()
    {
        return glm::ortho(
            -6.4f,
            6.4f,
            -3.6f,
            3.6f,
            .1f,
            100.0f
        );
    }

    // 3D 위치에 맞춰 이름·순위 글자를 배치.
    glm::vec2 ScreenPoint(glm::vec3 p)
    {
        const glm::vec4 clip =
            PodiumProjection() *
            PodiumView() *
            glm::vec4(p, 1);

        return {
            (clip.x / clip.w + 1) * W * .5f,
            (1 - clip.y / clip.w) * H * .5f
        };
    }

    float PodiumX(int rank)
    {
        return
            rank == 1 ? 0.0f :
            rank == 2 ? -3.0f :
            3.0f;
    }

    float PodiumHeight(int rank)
    {
        return
            rank == 1 ? 2.15f :
            rank == 2 ? 1.45f :
            .8f;
    }

    // 폰트에 없는 왕관 문자는 도형으로 그림.
    void Crown(float x, float y)
    {
        glDisable(GL_TEXTURE_2D);
        glColor3f(1, .78f, .15f);

        glBegin(GL_TRIANGLES);

        glVertex2f(x - 28, y + 12);
        glVertex2f(x - 33, y - 16);
        glVertex2f(x, y + 12);

        glVertex2f(x - 22, y + 12);
        glVertex2f(x, y - 26);
        glVertex2f(x + 22, y + 12);

        glVertex2f(x, y + 12);
        glVertex2f(x + 33, y - 16);
        glVertex2f(x + 28, y + 12);

        glEnd();

        Box(
            { x - 28, y + 10, 56, 9 },
            1,
            .69f,
            .08f
        );
    }

    void Podium()
    {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);

        glDisable(GL_BLEND);

        glUseProgram(shaderProgramID);
        glBindVertexArray(vao);

        const glm::mat4 projection = PodiumProjection();

        const glm::mat4 view = PodiumView();
        // 결과 화면이 기존 게임의 그림자 맵에 영향받지 않도록 함.
        const glm::mat4 light =
            glm::translate(
                glm::mat4(1),
                glm::vec3(0, 0, 100)
            );

        auto matrix = [](
            const char* name,
            const glm::mat4& m
            )
            {
                glUniformMatrix4fv(
                    glGetUniformLocation(
                        shaderProgramID,
                        name
                    ),
                    1,
                    GL_FALSE,
                    glm::value_ptr(m)
                );
            };

        matrix("projection", projection);
        matrix("view", view);
        matrix("lightSpaceMatrix", light);

        glUniform3f(glGetUniformLocation(shaderProgramID, "lightPos"), -3, 8, 7);
        glUniform1i(glGetUniformLocation(shaderProgramID, "useTexture"),0);
        glUniform1i(glGetUniformLocation(shaderProgramID, "enableSpotlight"), 0);
        glUniform1i(glGetUniformLocation(shaderProgramID, "isGlitch"),0);

        // 시상대 바닥.
        drawPart(
            shaderProgramID,
            glm::mat4(1),
            { 0, -.1f, 0 },
            { 10, .2f, 2.4f },
            { .16f, .25f, .30f }
        );

        for (const auto& e : entries) {
            const float x = PodiumX(e.rank);
            const float h = PodiumHeight(e.rank);

            const glm::vec3 color =
                e.rank == 1
                ? glm::vec3(.60f, .41f, .08f)
                : e.rank == 2
                ? glm::vec3(.36f, .44f, .50f)
                : glm::vec3(.43f, .24f, .13f);

            drawPart(
                shaderProgramID,
                glm::mat4(1),
                { x, h * .5f, 0 },
                { 2.6f, h, 1.8f },
                color
            );

            // 모델 출력은 이 함수 한 곳을 통해 호출.
            DrawResultPlayer(
                e.playerId,
                { x, h, 0 },
                1.3f
            );
        }

        Begin2D();

        Text(640, 78, "RESULT", .85f);

        for (const auto& e : entries) {
            const float x = PodiumX(e.rank);
            const float h = PodiumHeight(e.rank);
            const auto name = ScreenPoint({ x, h + 1.65f, 0 });

            Text(
                name.x,
                name.y,
                PlayerName(e),
                .50f,
                255
            );

            const auto rank = ScreenPoint({ x, h * .47f, 1.0f });

            Text(rank.x, rank.y + 10, RankName(e.rank), .65f, 210);

            if (e.rank == 1) {
                Crown(
                    name.x,
                    name.y - 65
                );
            }
        }

        Button(nextButton, "NEXT >");
    }

    // 최대 거리 / 최다 사용 카드.
    // 같은 기록이면 해당 플레이어를 함께 표시.
    std::string Award(bool distance)
    {
        int best = 0;

        for (const auto& e : entries) {
            best = (std::max)(
                best,
                distance ? e.distance : e.used
                );
        }

        if (best == 0) {
            return "NONE";
        }

        std::string names;

        for (const auto& e : entries) {
            const int value = distance ? e.distance : e.used;
            if (value != best) {
                continue;
            }

            if (!names.empty()) {
                names += ", ";
            }

            names += std::to_string(e.slot) + "P";
        }

        return
            names + " / " +
            std::to_string(best);
    }

    void Stats()
    {
        Text(640, 78, "STATS", .85f);

        Box({ 90, 130, 1100, 300 }, .09f, .17f, .23f);
        Box({ 90, 130, 1100, 62 }, .16f, .28f, .34f);
        const float cols[] = {270, 610, 820, 1040};

        const char* labels[] = {"PLAYER", "DIST", "TIME", "USED"};

        for (int i = 0; i < 4; ++i) {
            Text(cols[i], 174, labels[i], .50f, 240);
        }

        for (std::size_t i = 0; i < entries.size(); ++i)
        {
            const auto& e = entries[i];
            const float y = 251.0f + i * 76.0f;
            const std::string name = std::string(RankName(e.rank)) + " " + std::to_string(e.slot) +
                "P" + (e.playerId == previewMyId ? " (YOU)" : "");

            Text(cols[0], y, name, .50f, 310);
            Text(cols[1], y, std::to_string(e.distance));
            Text(cols[2], y, std::to_string(e.seconds) + "s");
            Text(cols[3], y, std::to_string(e.used));
        }

        Box( { 130, 466, 470, 112 }, .14f, .25f, .30f);
        Box({ 680, 466, 470, 112 }, .14f, .25f, .30f);

        Text(365, 506, "MOST DIST", .48f, 430);
        Text(915, 506, "MOST USED", .48f, 430);
        Text(365, 552, Award(true), .55f, 430);
        Text(915, 552, Award(false), .55f, 430);

        Button(backButton, "< BACK");
        Button(okButton, "OK");
    }

}

bool IsResultPreviewActive()
{
    return active;
}

void OpenResultPreview()
{
    // 1일차 미리보기는 오프라인 타이틀에서만 허용.
    if (clientScreen != ClientScreen::Title ||
        activeClientNetwork != nullptr)
    {
        return;
    }

    page = Page::Podium;
    active = true;

    glutPostRedisplay();
}

void DrawResultPlayer(std::uint32_t playerId, const glm::vec3& footPosition, float scale)
{
    const Entry* entry = nullptr;
    for (const auto& e : entries) {
        if (e.playerId == playerId) {
            entry = &e;
            break;
        }
    }

    if (!entry) {
        return;
    }

    // 나중에 entry->appearanceId에 따라
    // 선택된 캐릭터 모델을 그리도록 이곳에서 연결.

    // 기존 닭 모델은 발바닥이 원점보다 0.5 아래에 있음.
    // 따라서 발 위치가 시상대 위에 오도록 보정.
    glm::mat4 model = glm::translate(glm::mat4(1), footPosition + glm::vec3(0, .5f * scale, 0));
    model = glm::rotate(model, glm::radians(135.0f), glm::vec3(0, 1, 0));
    model = glm::scale(model, glm::vec3(scale));

    drawChicken(shaderProgramID, model);
}

void DrawResultPreview()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_TRUE);

    glViewport(0, 0, glutGet(GLUT_WINDOW_WIDTH), glutGet(GLUT_WINDOW_HEIGHT));

    glClearColor(.035f, .07f,.10f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const auto v = FitViewport();

    glViewport(v.x, v.y, v.w, v.h);

    Begin2D();

    if (page == Page::Podium) {
        Podium();
    }
    else {
        Stats();
    }
}

void HandleResultPreviewKey(unsigned char key)
{
    if (key == 27) {
        // ESC: 미리보기 종료.
        active = false;
    }
    else if (key == 13) {
        // Enter: NEXT 또는 OK.
        if (page == Page::Podium) {
            page = Page::Stats;
        }
        else {
            active = false;
        }
    }
    else if (key == 8 &&
        page == Page::Stats)
    {
        // Backspace: 순위 화면으로 돌아가기.
        page = Page::Podium;
    }

    glutPostRedisplay();
}

void HandleResultPreviewMouse(int button, int state, int x, int y)
{
    if (button != GLUT_LEFT_BUTTON ||
        state != GLUT_DOWN)
    {
        return;
    }
    const auto v = FitViewport();
    const int top = glutGet(GLUT_WINDOW_HEIGHT) - v.y - v.h;
    // 실제 창 좌표 → 결과 UI 좌표.
    const float ux = (x - v.x) * W / v.w;

    const float uy = (y - top) * H / v.h;
    if (page == Page::Podium) {
        if (Contains(nextButton, ux, uy)) {
            page = Page::Stats;
        }
    }
    else {
        if (Contains(backButton, ux, uy)) {
            page = Page::Podium;
        }
        else if (Contains(okButton, ux, uy)) {
            active = false;
        }
    }

    glutPostRedisplay();
}