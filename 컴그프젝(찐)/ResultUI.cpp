#include "ResultUI.h"
#include "Global.h"
#include "Renderer.h"
#include "Logic.h"
#include "ClientNetwork.h"

#include <array>
#include <cstdio>

namespace {

    constexpr float W = 1280.0f;
    constexpr float H = 720.0f;

    enum class Page {
        Podium,
        Stats,
        Rematch
    };

    bool active = false;
    Page page = Page::Podium;

    using Entry = ResultEntry;

    std::array<Entry, 3> entries{};

    std::uint32_t myPlayerId = 0;
    std::uint32_t shownRound = 0;

    bool previewMode = false;

    void ConfirmResult()
    {
        if (!active) return;
        if (previewMode) {
            CloseResultScreen();
            return;
        }
        if (!activeClientNetwork) return;
        // 방이 해산되어도 결과는 확인할 때까지 보관
        if (activeClientNetwork->GetResult().roomClosing) {
            if (activeClientNetwork->ConfirmResult()) {
                CloseResultScreen();
                returnToTitle();
            }
        }
        // STATS의 OK를 누른 참가자만 개인별 응답 시간 시작
        else if (activeClientNetwork->RequestRematchOpen() ||
            activeClientNetwork->GetLobby().rematchPromptOpen) {
            page = Page::Rematch;
        }
        glutPostRedisplay();
    }

    std::string Seconds(std::uint32_t ticks)
    {
        char buffer[32];

        std::snprintf(
            buffer,
            sizeof(buffer),
            "%.1fs",
            static_cast<double>(ticks) / Wire::TickRate
        );

        return buffer;
    }

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
                width += cdata[c - 32].xadvance * scale;
            }
        }

        return width;
    }

    void Text(
        float cx,
        float baseline,
        const std::string& text,
        float scale = .55f,
        float maxWidth = 1100
    )
    {
        const float width =
            TextWidth(text, scale);

        if (width > maxWidth) {
            scale *= maxWidth / width;
        }

        const float start =
            cx - TextWidth(text, scale) * .5f;

        glActiveTexture(GL_TEXTURE0);
        glEnable(GL_TEXTURE_2D);

        glBindTexture(
            GL_TEXTURE_2D,
            fontTexture
        );

        glTexEnvi(
            GL_TEXTURE_ENV,
            GL_TEXTURE_ENV_MODE,
            GL_MODULATE
        );

        glColor3f(1, 1, 1);

        float penX = 0;
        float penY = 0;

        glBegin(GL_QUADS);

        for (unsigned char c : text) {
            if (c < 32 || c >= 128) {
                continue;
            }

            stbtt_aligned_quad q;

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

    void Button(const Rect& r, const char* label)
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
            (e.playerId == myPlayerId ? "YOU: " : "") +
            std::to_string(
                static_cast<unsigned>(e.slot) + 1
            ) +
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

    glm::vec2 ScreenPoint(glm::vec3 position)
    {
        const glm::vec4 clip =
            PodiumProjection() *
            PodiumView() *
            glm::vec4(position, 1);

        return {
            (clip.x / clip.w + 1) * W * .5f,
            (1 - clip.y / clip.w) * H * .5f
        };
    }

    // 가로 위치는 표시 순서로 결정.
    // 공동 순위여도 같은 위치에 겹치지 않음.
    float PodiumX(std::size_t index)
    {
        return
            index == 0 ? 0.0f :
            index == 1 ? -3.0f :
            3.0f;
    }

    // 높이는 실제 서버 순위로 결정.
    float PodiumHeight(int rank)
    {
        return
            rank == 1 ? 2.15f :
            rank == 2 ? 1.45f :
            .8f;
    }

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

        const glm::mat4 projection =
            PodiumProjection();

        const glm::mat4 view =
            PodiumView();

        const glm::mat4 light =
            glm::translate(
                glm::mat4(1),
                glm::vec3(0, 0, 100)
            );

        auto matrix = [](
            const char* name,
            const glm::mat4& value
            )
            {
                glUniformMatrix4fv(
                    glGetUniformLocation(
                        shaderProgramID,
                        name
                    ),
                    1,
                    GL_FALSE,
                    glm::value_ptr(value)
                );
            };

        matrix("projection", projection);
        matrix("view", view);
        matrix("lightSpaceMatrix", light);

        glUniform3f(
            glGetUniformLocation(
                shaderProgramID,
                "lightPos"
            ),
            -3, 8, 7
        );

        glUniform1i(
            glGetUniformLocation(
                shaderProgramID,
                "useTexture"
            ),
            0
        );

        glUniform1i(
            glGetUniformLocation(
                shaderProgramID,
                "enableSpotlight"
            ),
            0
        );

        glUniform1i(
            glGetUniformLocation(
                shaderProgramID,
                "isGlitch"
            ),
            0
        );

        drawPart(
            shaderProgramID,
            glm::mat4(1),
            { 0, -.1f, 0 },
            { 10, .2f, 2.4f },
            { .16f, .25f, .30f }
        );

        for (std::size_t i = 0;
            i < entries.size();
            ++i)
        {
            const auto& e = entries[i];

            const float x = PodiumX(i);
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

            DrawResultPlayer(
                e.playerId,
                { x, h, 0 },
                1.3f
            );
        }

        Begin2D();

        Text(640, 78, "RESULT", .85f);

        for (std::size_t i = 0;
            i < entries.size();
            ++i)
        {
            const auto& e = entries[i];

            const float x = PodiumX(i);
            const float h = PodiumHeight(e.rank);

            const auto name =
                ScreenPoint({ x, h + 1.65f, 0 });

            Text(
                name.x,
                name.y,
                PlayerName(e),
                .50f,
                255
            );

            const auto rank =
                ScreenPoint({ x, h * .47f, 1.0f });

            Text(
                rank.x,
                rank.y + 10,
                RankName(e.rank),
                .65f,
                210
            );

            if (e.rank == 1) {
                Crown(name.x, name.y - 65);
            }
        }

        Button(nextButton, "NEXT >");
    }

    std::string Award(bool distance)
    {
        std::uint32_t best = 0;

        for (const auto& e : entries) {
            best = (std::max)(
                best,
                distance
                ? e.distanceLanes
                : e.itemsUsed
                );
        }

        if (best == 0) {
            return "NONE";
        }

        std::string names;

        for (const auto& e : entries) {
            const auto value =
                distance
                ? e.distanceLanes
                : e.itemsUsed;

            if (value != best) {
                continue;
            }

            if (!names.empty()) {
                names += ", ";
            }

            names +=
                std::to_string(
                    static_cast<unsigned>(e.slot) + 1
                ) +
                "P";
        }

        return
            names + " / " +
            std::to_string(best);
    }

    void Stats()
    {
        Text(640, 78, "STATS", .85f);

        Box(
            { 90, 130, 1100, 300 },
            .09f, .17f, .23f
        );

        Box(
            { 90, 130, 1100, 62 },
            .16f, .28f, .34f
        );

        const float cols[] = {
            270,
            610,
            820,
            1040
        };

        const char* labels[] = {
            "PLAYER",
            "DIST",
            "TIME",
            "USED"
        };

        for (int i = 0; i < 4; ++i) {
            Text(
                cols[i],
                174,
                labels[i],
                .50f,
                240
            );
        }

        for (std::size_t i = 0;
            i < entries.size();
            ++i)
        {
            const auto& e = entries[i];

            const float y =
                251.0f + i * 76.0f;

            const std::string name =
                std::string(RankName(e.rank)) +
                " " +
                std::to_string(
                    static_cast<unsigned>(e.slot) + 1
                ) +
                "P" +
                (e.playerId == myPlayerId ? " (YOU)" : "") +
                (e.departed ? " LEFT" : "");

            Text(
                cols[0],
                y,
                name,
                .50f,
                310
            );

            Text(
                cols[1],
                y,
                std::to_string(e.distanceLanes)
            );

            Text(
                cols[2],
                y,
                Seconds(e.survivalTicks)
            );

            Text(
                cols[3],
                y,
                std::to_string(e.itemsUsed)
            );
        }

        Box(
            { 130, 466, 470, 112 },
            .14f, .25f, .30f
        );

        Box(
            { 680, 466, 470, 112 },
            .14f, .25f, .30f
        );

        Text(365, 506, "MOST DIST", .48f, 430);
        Text(915, 506, "MOST USED", .48f, 430);

        Text(365, 552, Award(true), .55f, 430);
        Text(915, 552, Award(false), .55f, 430);

        Button(backButton, "< BACK");

        Button(
            okButton,
            "OK"
        );
    }

    // 기존 버튼과 글꼴을 재사용하여 서버 확정 투표 상태 표시
    void Rematch()
    {
        if (!activeClientNetwork) return;
        const auto& lobby = activeClientNetwork->GetLobby();
        Text(640, 220, "PLAY AGAIN?", .65f, 400);
        if (lobby.rematchSubmitted) {
            Text(640, 350, "REQUEST SENT", .4f, 300);
            Text(640, 410, "WAITING FOR OTHER PLAYERS", .3f, 420);
        }
        else {
            Text(640, 350,
                std::to_string(activeClientNetwork->GetRematchSecondsLeft()) + "s",
                .4f, 140);
            Button(backButton, "Y : YES");
            Button(okButton, "N : NO");
        }
        for (std::size_t i = 0; i < entries.size(); ++i) {
            // 순위 정렬 후에도 플레이어 번호로 서버 확정 투표 연결
            const auto& received = activeClientNetwork->GetResult().entries;
            const auto member = std::find_if(received.begin(), received.end(),
                [&](const Entry& entry) { return entry.playerId == entries[i].playerId; });
            const auto value = member == received.end() ? RematchVote::UNSET :
                lobby.rematchVotes[static_cast<std::size_t>(member - received.begin())];
            const char* vote = value == RematchVote::AGREE ? "YES" :
                value == RematchVote::DISAGREE ? "NO" : "WAIT";
            Text(640, 480 + static_cast<float>(i) * 40,
                std::to_string(entries[i].playerId) + "P : " + vote, .3f, 250);
        }
    }

} // namespace

bool IsResultScreenActive()
{
    return active;
}

void OpenResultScreen(const ResultData& data, std::uint32_t selfId)
{
    if (data.roundId == 0 || selfId == 0) {
        return;
    }

    // 같은 경기 결과를 매 프레임 다시 초기화하지 않음.
    if (active &&
        !previewMode &&
        shownRound == data.roundId &&
        myPlayerId == selfId)
    {
        return;
    }

    // 네트워크 자료형을 참조로 붙들지 않고 값으로 보관.
    entries = data.entries;

    // 서버가 정한 순위는 유지.
    // 동률끼리의 표시 순서만 슬롯 기준으로 고정.
    std::stable_sort(
        entries.begin(),
        entries.end(),
        [](const Entry& a, const Entry& b) {
            return a.rank != b.rank
                ? a.rank < b.rank
                : a.slot < b.slot;
        }
    );

    shownRound = data.roundId;
    myPlayerId = selfId;

    previewMode = false;

    page = Page::Podium;
    active = true;

    glutPostRedisplay();
}

void OpenResultPreview()
{
    if (clientScreen != ClientScreen::Title ||
        activeClientNetwork != nullptr)
    {
        return;
    }

    // 오프라인 F6 미리보기 전용 데이터.
    entries = { {
        {3, 92, 1560, 2, 2, 1, false},
        {1, 80, 1350, 3, 0, 2, false},
        {2, 65, 1200, 6, 1, 3, false}
    } };

    shownRound = 0;
    myPlayerId = 1;

    previewMode = true;

    page = Page::Podium;
    active = true;

    glutPostRedisplay();
}

void CloseResultScreen()
{
    active = false;
    previewMode = false;

    shownRound = 0;
    myPlayerId = 0;
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

    // 커스터마이징 통합 시 모델 선택은 이곳에서 연결.
    glm::mat4 model = glm::translate(glm::mat4(1), footPosition + glm::vec3(0, .5f * scale, 0));

    const double seconds = glutGet(GLUT_ELAPSED_TIME) * 0.001;

    // 초당 20도: 약 18초에 한 바퀴.
    const float angleDegrees =
        static_cast<float>(
            std::fmod(135.0 + seconds * 20.0, 360.0)
            );

    model = glm::rotate(
        model,
        glm::radians(angleDegrees),
        glm::vec3(0, 1, 0)
    );

    model = glm::scale(
        model,
        glm::vec3(scale)
    );

    drawChicken(shaderProgramID, model);
}

void DrawResultScreen()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_TRUE);

    glViewport(
        0,
        0,
        glutGet(GLUT_WINDOW_WIDTH),
        glutGet(GLUT_WINDOW_HEIGHT)
    );

    glClearColor(.035f, .07f, .10f, 1);

    glClear(
        GL_COLOR_BUFFER_BIT |
        GL_DEPTH_BUFFER_BIT
    );

    const auto v = FitViewport();

    glViewport(v.x, v.y, v.w, v.h);

    Begin2D();

    if (!previewMode && activeClientNetwork &&
        activeClientNetwork->GetResult().roomClosing && page == Page::Rematch) {
        page = Page::Stats;
    }
    if (page == Page::Rematch) {
        Rematch();
    }
    else if (page == Page::Podium) {
        Podium();
    }
    else {
        Stats();
        if (!previewMode && activeClientNetwork &&
            activeClientNetwork->GetResult().roomClosing) {
            Text(640, 610, activeClientNetwork->GetLobby().message, .22f, 800);
        }
    }


}

void HandleResultKey(unsigned char key)
{
    if (page == Page::Rematch) {
        if (activeClientNetwork) {
            if (key == 'y' || key == 'Y') activeClientNetwork->RequestRematch(true);
            else if (key == 'n' || key == 'N') activeClientNetwork->RequestRematch(false);
        }
        if (key == 'q' || key == 'Q') glutLeaveMainLoop();
        return;
    }
    if (key == 27 && previewMode) {
        // ESC는 오프라인 미리보기만 종료.
        CloseResultScreen();
    }
    else if (key == 13) {
        if (page == Page::Podium) {
            page = Page::Stats;
        }
        else {
            ConfirmResult();
        }
    }
    else if (key == 8 &&
        page == Page::Stats)
    {
        page = Page::Podium;
    }

    glutPostRedisplay();
}

void HandleResultMouse(int button, int state, int x, int y)
{
    if (button != GLUT_LEFT_BUTTON ||
        state != GLUT_DOWN)
    {
        return;
    }

    const auto v = FitViewport();
    const int top = glutGet(GLUT_WINDOW_HEIGHT) - v.y - v.h;
    const float ux = (x - v.x) * W / v.w;
    const float uy = (y - top) * H / v.h;

    if (page == Page::Rematch) {
        if (activeClientNetwork) {
            if (Contains(backButton, ux, uy)) activeClientNetwork->RequestRematch(true);
            else if (Contains(okButton, ux, uy)) activeClientNetwork->RequestRematch(false);
        }
    }
    else if (page == Page::Podium) {
        if (Contains(nextButton, ux, uy)) {
            page = Page::Stats;
        }
    }
    else {
        if (Contains(backButton, ux, uy)) {
            page = Page::Podium;
        }
        else if (Contains(okButton, ux, uy)) {
            ConfirmResult();
        }
    }

    glutPostRedisplay();
}