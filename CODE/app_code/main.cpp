#include "raylib.h"
#include <vector>
#include <iostream>
#include <cmath>

struct PunctTraseu {
    float x;
    float y;
};

int main() {
    const int screenWidth = 800;
    const int screenHeight = 900;
    const float scaleFactor = 2.5f;

    InitWindow(screenWidth, screenHeight, "Sistem mobil cu traseu programabil");
    SetTargetFPS(60);

    std::vector<PunctTraseu> listaPuncte;
    listaPuncte.push_back({400.0f, 400.0f});

    // animatie
    bool robotInMiscare = false;
    Vector2 robotPos = { 400.0f, 400.0f }; // poz robotului
    float robotAngle = -PI / 2.0f;         // unghi de rotatie
    int targetIndex = 1;                   // urmatorul punct
    const float vitezaRobot = 200.0f;      // pixel/s

    // camlock
    Camera2D camera = { 0 };
    camera.target = { 400.0f, 400.0f };
    camera.offset = { 400.0f, 400.0f };
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    // UI
    Rectangle butonActiune = { screenWidth / 2.0f - 75, 825, 150, 50 };
    Rectangle panouText = { 0, 0, 260, 70 };

    while (!WindowShouldClose()) {
        Vector2 mousePos = GetMousePosition();

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (CheckCollisionPointRec(mousePos, butonActiune)) {
                if (!robotInMiscare) {
                    if (listaPuncte.size() > 1) {
                        robotInMiscare = true;
                        std::cout << "Start animatie, trimitere date catre microcontroller" << std::endl;
                    }
                } else {
                    robotInMiscare = false;
                    listaPuncte.clear();
                    listaPuncte.push_back({400.0f, 400.0f});

                    robotPos = { 400.0f, 400.0f };
                    robotAngle = -PI / 2.0f;
                    targetIndex = 1;
                    camera.target = { 400.0f, 400.0f };

                    std::cout << "Reset finalizat" << std::endl;
                }
            }
            else if (!CheckCollisionPointRec(mousePos, panouText) &&
                     !robotInMiscare &&
                     listaPuncte.size() < 10 &&
                     mousePos.y <= 800) {

                listaPuncte.push_back({mousePos.x, mousePos.y});
            }
        }

        // animatie
        if (robotInMiscare && targetIndex < listaPuncte.size()) {
            Vector2 target = { listaPuncte[targetIndex].x, listaPuncte[targetIndex].y };

            // calculam distanta
            float dx = target.x - robotPos.x;
            float dy = target.y - robotPos.y;
            float distanta = sqrt(dx*dx + dy*dy); // pitagora

            float moveStep = vitezaRobot * GetFrameTime();

            if (distanta <= moveStep) {
                // daca e destul de aproape de punct, se lipeste de el si apoi trece la urmatorul
                robotPos = target;
                targetIndex++;
            } else {
                robotPos.x += (dx / distanta) * moveStep;
                robotPos.y += (dy / distanta) * moveStep;

                // reconfiguram directia robotului
                robotAngle = atan2(dy, dx);
            }

            camera.target = robotPos;
        }

        // desenare
        BeginDrawing();

        ClearBackground({ 200, 200, 200, 255 });

        BeginMode2D(camera);

            // Fundalul zonei de 2x2m
            DrawRectangle(0, 0, 800, 800, { 245, 245, 245, 255 });

            // Axa X si Y
            DrawLine(400, 0, 400, 800, { 220, 220, 220, 255 });
            DrawLine(0, 400, 800, 400, { 220, 220, 220, 255 });
            DrawRectangleLines(0, 0, 800, 800, DARKGRAY); // Conturul podelei

            // Desenăm traseul stabilit
            for (size_t i = 0; i < listaPuncte.size(); i++) {
                if (i > 0) {
                    DrawLineEx({listaPuncte[i-1].x, listaPuncte[i-1].y},
                               {listaPuncte[i].x, listaPuncte[i].y}, 3.0f, robotInMiscare ? GRAY : BLUE);
                }

                // Desenam punctele destinatie
                if (i != 0) {
                    DrawCircle(listaPuncte[i].x, listaPuncte[i].y, 6, robotInMiscare ? DARKGRAY : RED);
                }
            }

            // desenam robotul la pozitia lui actuala
            DrawCircle(robotPos.x, robotPos.y, 10, DARKGREEN);
            float fataX = robotPos.x + cos(robotAngle) * 25.0f;
            float fataY = robotPos.y + sin(robotAngle) * 25.0f;
            DrawLineEx({robotPos.x, robotPos.y}, {fataX, fataY}, 4.0f, DARKGREEN);

        EndMode2D(); // Oprim camera aici

        // UI
        // Bara de jos
        DrawRectangle(0, 800, 800, 100, RAYWHITE);
        DrawLine(0, 800, 800, 800, DARKGRAY);

        // Panoul de text de sus
        DrawRectangleRec(panouText, Fade(RAYWHITE, 0.90f));
        DrawText(TextFormat("Puncte adaugate: %d / 9", listaPuncte.size() - 1), 10, 10, 20, DARKGRAY);
        if (listaPuncte.size() >= 10 && !robotInMiscare) {
            DrawText("LIMITA ATINSA!", 10, 40, 20, ORANGE);
        }

        else if (targetIndex == listaPuncte.size() && targetIndex != 1) {
            DrawText("Traseu finalizat", 10, 40, 20, GREEN);
        }

        // Butonul de Start/Reset
        if (!robotInMiscare) {
            DrawRectangleRec(butonActiune, listaPuncte.size() > 1 ? DARKGREEN : GRAY);
            DrawText("START", butonActiune.x + 45, butonActiune.y + 15, 20, WHITE);
        } else {
            DrawRectangleRec(butonActiune, MAROON);
            DrawText("RESET", butonActiune.x + 45, butonActiune.y + 15, 20, WHITE);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}