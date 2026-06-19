#include "raylib.h"
#include "functions.h"
#include <vector>
#include <string>
#include <iostream>
#include <cmath>
#include <queue>
#include <algorithm>

struct PunctTraseu {
    float x;
    float y;
};

std::vector<PunctTraseu> ExtrageTraseuDinImagine(const std::string& caleFisier) {
    std::vector<PunctTraseu> puncteFiltrate;
    if (caleFisier.empty()) return puncteFiltrate;

    Image img = LoadImage(caleFisier.c_str());
    if (img.data == nullptr) return puncteFiltrate;

    Color* pixeli = LoadImageColors(img);
    int w = img.width;
    int h = img.height;

    // Gasim primul pixel de negru
    int startX = -1, startY = -1;
    bool gasitStart = false;
    for (int y = 0; y < h && !gasitStart; y++) {
        for (int x = 0; x < w; x++) {
            int idx = y * w + x;
            // Prag de culoare pentru linii negre sau gri inchise
            if (pixeli[idx].r < 140 && pixeli[idx].g < 140 && pixeli[idx].b < 140) {
                startX = x;
                startY = y;
                gasitStart = true;
                break;
            }
        }
    }

    if (!gasitStart) {
        UnloadImageColors(pixeli);
        UnloadImage(img);
        return puncteFiltrate;
    }

    // Mapam toata linia folosind BFS
    std::vector<int> dist(w * h, -1);
    std::queue<std::pair<int, int>> q;

    dist[startY * w + startX] = 0;
    q.push({startX, startY});

    int maxDist = 0;
    int endX = startX;
    int endY = startY;

    // Directii 8-conectate pentru deplasare pixel cu pixel
    int dx8[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    int dy8[] = {-1, -1, -1, 0, 0, 1, 1, 1};

    while (!q.empty()) {
        auto curent = q.front();
        q.pop();

        int cx = curent.first;
        int cy = curent.second;
        int cDist = dist[cy * w + cx];

        // Retinem pixelul cel mai indepartat
        if (cDist > maxDist) {
            maxDist = cDist;
            endX = cx;
            endY = cy;
        }

        for (int i = 0; i < 8; i++) {
            int nx = cx + dx8[i];
            int ny = cy + dy8[i];

            if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                int nIdx = ny * w + nx;
                // Dacă e pixel negru și nu a fost vizitat încă
                if (dist[nIdx] == -1 && pixeli[nIdx].r < 140 && pixeli[nIdx].g < 140 && pixeli[nIdx].b < 140) {
                    dist[nIdx] = cDist + 1;
                    q.push({nx, ny});
                }
            }
        }
    }

    // Reconstruim traseul de la capat la inceput
    std::vector<PunctTraseu> toatePuncteleLiniei;
    int tx = endX;
    int ty = endY;

    while (true) {
        toatePuncteleLiniei.push_back({(float)tx, (float)ty});
        if (tx == startX && ty == startY) break;

        int urmatorulX = tx;
        int urmatorulY = ty;
        int minVecinDist = dist[ty * w + tx];

        // Cautam vecinul cu cea mai mica distanta BFS
        for (int i = 0; i < 8; i++) {
            int nx = tx + dx8[i];
            int ny = ty + dy8[i];

            if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                int nIdx = ny * w + nx;
                if (dist[nIdx] != -1 && dist[nIdx] < minVecinDist) {
                    minVecinDist = dist[nIdx];
                    urmatorulX = nx;
                    urmatorulY = ny;
                }
            }
        }

        // Protecție anti blocare
        if (urmatorulX == tx && urmatorulY == ty) break;

        tx = urmatorulX;
        ty = urmatorulY;
    }

    // Inversam vectorul ca am mers de la capat la inceput
    std::reverse(toatePuncteleLiniei.begin(), toatePuncteleLiniei.end());

    UnloadImageColors(pixeli);
    UnloadImage(img);

    // Compresie la fix 10 puncte egale si centrate
    if (!toatePuncteleLiniei.empty()) {
        float offsetX = 400.0f - toatePuncteleLiniei[0].x;
        float offsetY = 400.0f - toatePuncteleLiniei[0].y;

        int totalPuncteRaw = toatePuncteleLiniei.size();

        if (totalPuncteRaw <= 10) {
            for (const auto& p : toatePuncteleLiniei) {
                puncteFiltrate.push_back({p.x + offsetX, p.y + offsetY});
            }
        } else {
            for (int i = 0; i <= 10; i++) {
                int idx = i * (totalPuncteRaw - 1) / 10;
                puncteFiltrate.push_back({
                    toatePuncteleLiniei[idx].x + offsetX,
                    toatePuncteleLiniei[idx].y + offsetY
                });
            }
        }
    }

    return puncteFiltrate;
}

int main() {
    // Dimensiuni fereastra interfata
    const int screenWidth = 800;
    const int screenHeight = 900;

    // Initializare fereastra Raylib si setare frame rate
    InitWindow(screenWidth, screenHeight, "Sistem mobil cu traseu programabil");
    SetTargetFPS(60);

    // Vector pentru punctele traseului (punctul de start e fixat la 400, 400)
    std::vector<PunctTraseu> listaPuncte;
    listaPuncte.push_back({400.0f, 400.0f});

    // Flag-uri si variabile stare pentru robot si simulare
    bool robotInMiscare = false;
    bool traseuImportat = false;
    Vector2 robotPos = { 400.0f, 400.0f };
    float robotAngle = -PI / 2.0f; // Orientat in sus initial
    int targetIndex = 1;
    const float vitezaRobot = 200.0f;

    // Configurare camera 2D pentru urmarirea robotului
    Camera2D camera = { 0 };
    camera.target = { 400.0f, 400.0f };
    camera.offset = { 400.0f, 400.0f };
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    // Pozitionare elemente UI (butoane si panouri)
    Rectangle butonActiune = { screenWidth / 2.0f - 160.0f, 825.0f, 150.0f, 50.0f };
    Rectangle butonIncarca = { screenWidth / 2.0f + 10.0f,  825.0f, 150.0f, 50.0f };
    Rectangle panouText = { 0.0f, 0.0f, 260.0f, 70.0f };

    // Bucla principala a aplicatiei
    while (!WindowShouldClose()) {
        Vector2 mousePos = GetMousePosition();

        // Tratare eveniment click stanga mouse
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {

            // Click pe butonul SEND / RESET
            if (CheckCollisionPointRec(mousePos, butonActiune)) {
                if (!robotInMiscare) {
                    if (listaPuncte.size() > 1) {
                        robotInMiscare = true;
                        std::string payload = "";
                        float currentAngleSimulation = -PI / 2.0f;

                        // Calculare comenzi relative (deplasari si rotiri) pentru hardware
                        for (size_t i = 1; i < listaPuncte.size(); i++) {
                            float dx = listaPuncte[i].x - listaPuncte[i-1].x;
                            float dy = listaPuncte[i].y - listaPuncte[i-1].y;

                            float distantaPixeli = sqrt(dx * dx + dy * dy);
                            float distantaCm = distantaPixeli * 0.15f; // Scara conversie px in cm

                            float unghiAbsolut = atan2(dy, dx);
                            float deltaUnghi = unghiAbsolut - currentAngleSimulation;

                            // Incadrare unghi in intervalul [-PI, PI]
                            while (deltaUnghi > PI) deltaUnghi -= 2 * PI;
                            while (deltaUnghi < -PI) deltaUnghi += 2 * PI;

                            payload += "R:" + std::to_string(deltaUnghi) + ";M:" + std::to_string(distantaCm) + "|";
                            currentAngleSimulation = unghiAbsolut;
                        }
                        payload += "END\n";

                        // Trimitere payload string prin thread separat pe portul COM
                        SendBluetoothThreaded("\\\\.\\COM5", payload);
                    }
                } else {
                    // Resetare completa simulare la starea initiala
                    robotInMiscare = false;
                    traseuImportat = false;
                    listaPuncte.clear();
                    listaPuncte.push_back({400.0f, 400.0f});
                    robotPos = { 400.0f, 400.0f };
                    robotAngle = -PI / 2.0f;
                    targetIndex = 1;
                    camera.target = { 400.0f, 400.0f };
                }
            }
            // Click pe butonul LOAD IMAGE (Import traseu din exterior)
            else if (CheckCollisionPointRec(mousePos, butonIncarca) && !robotInMiscare) {
                std::string caleFisier = OpenFileDialog();
                if (!caleFisier.empty()) {
                    // Apelare algoritm BFS de procesare imagine
                    std::vector<PunctTraseu> puncteNoi = ExtrageTraseuDinImagine(caleFisier);
                    if (!puncteNoi.empty()) {
                        listaPuncte = puncteNoi;
                        targetIndex = 1;
                        traseuImportat = true;
                    }
                }
            }
            // Click pe zona de grid pentru adaugare punct manual
            else if (!CheckCollisionPointRec(mousePos, panouText) &&
                     !robotInMiscare &&
                     listaPuncte.size() <= 10 &&
                     mousePos.y <= 800) {
                listaPuncte.push_back({mousePos.x, mousePos.y});
                traseuImportat = false;
            }
        }

        // Update logica miscare robot pe ecran (interpolare pozitii)
        if (robotInMiscare && targetIndex < listaPuncte.size()) {
            Vector2 target = { listaPuncte[targetIndex].x, listaPuncte[targetIndex].y };
            float dx = target.x - robotPos.x;
            float dy = target.y - robotPos.y;
            float distanta = sqrt(dx*dx + dy*dy);
            float moveStep = vitezaRobot * GetFrameTime();

            // Verificare daca robotul a atins punctul tinta curent
            if (distanta <= moveStep) {
                robotPos = target;
                targetIndex++;
            } else {
                // Deplasare efectiva si calcul unghi orientare
                robotPos.x += (dx / distanta) * moveStep;
                robotPos.y += (dy / distanta) * moveStep;
                robotAngle = atan2(dy, dx);
            }
            camera.target = robotPos; // Centrare camera pe robot
        }

        // Sectiunea de desenare grafica
        BeginDrawing();
        ClearBackground({ 245, 245, 245, 255 });
        BeginMode2D(camera);

            // Desenăm un grid extins (de la -4000 la 4000 px) ca să acopere tot ecranul la deplasare
            int dimensiuneCadran = 100;

            // Generare linii verticale pe toată suprafața mapată
            for (int x = -4000; x <= 4000; x += dimensiuneCadran) {
                Color culoareLinie = (x == 0 || x == 400 || x == 800) ? Color{ 210, 210, 210, 255 } : Color{ 232, 232, 232, 255 };
                DrawLine(x, -4000, x, 4000, culoareLinie);
            }

            // Generare linii orizontale pe toată suprafața mapată
            for (int y = -4000; y <= 4000; y += dimensiuneCadran) {
                Color culoareLinie = (y == 0 || y == 400 || y == 800) ? Color{ 210, 210, 210, 255 } : Color{ 232, 232, 232, 255 };
                DrawLine(-4000, y, 4000, y, culoareLinie);
            }

            // Evidențiere axe centrale originale (400, 400)
            DrawLineEx({ 400.0f, -4000.0f }, { 400.0f, 4000.0f }, 2.0f, { 190, 190, 190, 255 });
            DrawLineEx({ -4000.0f, 400.0f }, { 4000.0f, 400.0f }, 2.0f, { 190, 190, 190, 255 });

            // Desenare linii dintre puncte si cercuri pentru noduri
            for (size_t i = 0; i < listaPuncte.size(); i++) {
                if (i > 0) DrawLineEx({listaPuncte[i-1].x, listaPuncte[i-1].y}, {listaPuncte[i].x, listaPuncte[i].y}, 3.0f, robotInMiscare ? GRAY : BLUE);
                if (i != 0) DrawCircle(listaPuncte[i].x, listaPuncte[i].y, 6, robotInMiscare ? DARKGRAY : RED);
            }

            // Desenare robot (corp verde si linie directie directie fata)
            DrawCircle(robotPos.x, robotPos.y, 10, DARKGREEN);
            float fataX = robotPos.x + cos(robotAngle) * 25.0f;
            float fataY = robotPos.y + sin(robotAngle) * 25.0f;
            DrawLineEx({robotPos.x, robotPos.y}, {fataX, fataY}, 4.0f, DARKGREEN);
        EndMode2D();

        // Desenare elemente HUD / Interfata statica (independente de camera)
        DrawRectangle(0, 800, 800, 100, RAYWHITE);
        DrawLine(0, 800, 800, 800, DARKGRAY);
        DrawRectangleRec(panouText, Fade(RAYWHITE, 0.90f));

        // Afisare texte si indicatori de stare pe ecran
        DrawText(TextFormat("Puncte active: %d", listaPuncte.size() - 1), 10, 10, 20, DARKGRAY);
        DrawText("Scara: 1 Cadran = 0.6m (60cm)", 10, 40, 16, BLUE);

        if (targetIndex == listaPuncte.size() && targetIndex != 1) {
            DrawText("Traseu finalizat", 10, 65, 18, GREEN);
        } else if (traseuImportat) {
            DrawText("Traseu importat din Paint", 10, 65, 18, PURPLE);
        } else if (listaPuncte.size() >= 11 && !robotInMiscare) {
            DrawText("LIMITA CLICK ATINSA!", 10, 65, 18, ORANGE);
        }

        // Afisare buton SEND sau RESET in functie de starea robotului
        if (!robotInMiscare) {
            DrawRectangleRec(butonActiune, listaPuncte.size() > 1 ? DARKGREEN : GRAY);
            DrawText("SEND", butonActiune.x + 45, butonActiune.y + 15, 20, WHITE);
        } else {
            DrawRectangleRec(butonActiune, MAROON);
            DrawText("RESET", butonActiune.x + 45, butonActiune.y + 15, 20, WHITE);
        }

        // Afisare buton LOAD IMAGE
        DrawRectangleRec(butonIncarca, robotInMiscare ? GRAY : DARKBLUE);
        DrawText("LOAD IMAGE", butonIncarca.x + 15, butonIncarca.y + 15, 18, WHITE);

        EndDrawing();
    }

    // Eliberare resurse grafice si inchidere context Raylib
    CloseWindow();
    return 0;
}