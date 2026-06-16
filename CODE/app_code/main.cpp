#include "raylib.h"
#include "functions.h"
#include <vector>
#include <string>
#include <iostream>
#include <cmath>

struct PunctTraseu {
    float x;
    float y;
};

int main() {
    const int screenWidth = 800;
    const int screenHeight = 900;

    InitWindow(screenWidth, screenHeight, "Sistem mobil cu traseu programabil");
    SetTargetFPS(60);

    std::vector<PunctTraseu> listaPuncte;
    listaPuncte.push_back({400.0f, 400.0f});

    bool robotInMiscare = false;
    Vector2 robotPos = { 400.0f, 400.0f };
    float robotAngle = -PI / 2.0f;
    int targetIndex = 1;
    const float vitezaRobot = 200.0f;

    Camera2D camera = { 0 };
    camera.target = { 400.0f, 400.0f };
    camera.offset = { 400.0f, 400.0f };
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    Rectangle butonActiune = { screenWidth / 2.0f - 75.0f, 825.0f, 150.0f, 50.0f };
    Rectangle panouText = { 0.0f, 0.0f, 260.0f, 70.0f };

    while (!WindowShouldClose()) {
        Vector2 mousePos = GetMousePosition();

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (CheckCollisionPointRec(mousePos, butonActiune)) {
                if (!robotInMiscare) {
                    if (listaPuncte.size() > 1) {
                        robotInMiscare = true;

                        std::string payload = "";
                        float currentAngleSimulation = -PI / 2.0f;

                        for (size_t i = 1; i < listaPuncte.size(); i++) {
                            float dx = listaPuncte[i].x - listaPuncte[i-1].x;
                            float dy = listaPuncte[i].y - listaPuncte[i-1].y;

                            float distantaPixeli = sqrt(dx * dx + dy * dy);

                            // NOUA SCARĂ: Un pătrat de 400px înseamnă 60 cm reali (0.6m)
                            // distantaCm = distantaPixeli * (60.0cm / 400.0px)
                            float distantaCm = distantaPixeli * 0.15f;

                            float unghiAbsolut = atan2(dy, dx);
                            float deltaUnghi = unghiAbsolut - currentAngleSimulation;

                            while (deltaUnghi > PI) deltaUnghi -= 2 * PI;
                            while (deltaUnghi < -PI) deltaUnghi += 2 * PI;

                            payload += "R:" + std::to_string(deltaUnghi) + ";M:" + std::to_string(distantaCm) + "|";
                            currentAngleSimulation = unghiAbsolut;
                        }
                        payload += "END\n";

                        SendBluetoothThreaded("\\\\.\\COM5", payload);
                    }
                } else {
                    robotInMiscare = false;
                    listaPuncte.clear();
                    listaPuncte.push_back({400.0f, 400.0f});
                    robotPos = { 400.0f, 400.0f };
                    robotAngle = -PI / 2.0f;
                    targetIndex = 1;
                    camera.target = { 400.0f, 400.0f };
                }
            }
            else if (!CheckCollisionPointRec(mousePos, panouText) &&
                     !robotInMiscare &&
                     listaPuncte.size() < 10 &&
                     mousePos.y <= 800) {
                listaPuncte.push_back({mousePos.x, mousePos.y});
            }
        }

        if (robotInMiscare && targetIndex < listaPuncte.size()) {
            Vector2 target = { listaPuncte[targetIndex].x, listaPuncte[targetIndex].y };
            float dx = target.x - robotPos.x;
            float dy = target.y - robotPos.y;
            float distanta = sqrt(dx*dx + dy*dy);
            float moveStep = vitezaRobot * GetFrameTime();

            if (distanta <= moveStep) {
                robotPos = target;
                targetIndex++;
            } else {
                robotPos.x += (dx / distanta) * moveStep;
                robotPos.y += (dy / distanta) * moveStep;
                robotAngle = atan2(dy, dx);
            }
            camera.target = robotPos;
        }

        BeginDrawing();
        ClearBackground({ 200, 200, 200, 255 });
        BeginMode2D(camera);
            DrawRectangle(0, 0, 800, 800, { 245, 245, 245, 255 });
            DrawLine(400, 0, 400, 800, { 220, 220, 220, 255 });
            DrawLine(0, 400, 800, 400, { 220, 220, 220, 255 });
            DrawRectangleLines(0, 0, 800, 800, DARKGRAY);
            for (size_t i = 0; i < listaPuncte.size(); i++) {
                if (i > 0) DrawLineEx({listaPuncte[i-1].x, listaPuncte[i-1].y}, {listaPuncte[i].x, listaPuncte[i].y}, 3.0f, robotInMiscare ? GRAY : BLUE);
                if (i != 0) DrawCircle(listaPuncte[i].x, listaPuncte[i].y, 6, robotInMiscare ? DARKGRAY : RED);
            }
            DrawCircle(robotPos.x, robotPos.y, 10, DARKGREEN);
            float fataX = robotPos.x + cos(robotAngle) * 25.0f;
            float fataY = robotPos.y + sin(robotAngle) * 25.0f;
            DrawLineEx({robotPos.x, robotPos.y}, {fataX, fataY}, 4.0f, DARKGREEN);
        EndMode2D();

        DrawRectangle(0, 800, 800, 100, RAYWHITE);
        DrawLine(0, 800, 800, 800, DARKGRAY);
        DrawRectangleRec(panouText, Fade(RAYWHITE, 0.90f));
        DrawText(TextFormat("Puncte adaugate: %d / 9", listaPuncte.size() - 1), 10, 10, 20, DARKGRAY);

        // Afișăm pe ecran scara curentă ca să dea bine la prezentare
        DrawText("Scara: 1 Cadran = 0.6m (60cm)", 10, 40, 16, BLUE);

        if (listaPuncte.size() >= 10 && !robotInMiscare) {
            DrawText("LIMITA ATINSA!", 10, 65, 18, ORANGE);
        } else if (targetIndex == listaPuncte.size() && targetIndex != 1) {
            DrawText("Traseu finalizat", 10, 65, 18, GREEN);
        }

        if (!robotInMiscare) {
            DrawRectangleRec(butonActiune, listaPuncte.size() > 1 ? DARKGREEN : GRAY);
            DrawText("SEND", butonActiune.x + 45, butonActiune.y + 15, 20, WHITE);
        } else {
            DrawRectangleRec(butonActiune, MAROON);
            DrawText("RESET", butonActiune.x + 45, butonActiune.y + 15, 20, WHITE);
        }
        EndDrawing();
    }

    CloseWindow();
    return 0;
}