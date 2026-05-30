#include <SDL.h>
#include <SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>

#define NUM 6
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define FISHTANK_WIDTH 100
#define FISHTANK_HEIGHT 200

// 게임 상태 구조체 정의
typedef struct {
    float fish;       // 물고기 크기/성장도 (부드러운 연산을 위해 float 변경)
    float water;      // 어항 물 높이 (float 변경)
    int isAlive;      // 1: 살아있음, 0: 죽음
} FishTank;

FishTank fishTanks[NUM];    // 물고기 어항 배열
int level = 1;
int position = 0;
bool running = true;
bool gameOver = false;
bool gameWin = false;

unsigned int startTime = 0;
unsigned int lastUpdateTime = 0;

SDL_Window* window = NULL;          // SDL 창
SDL_Renderer* renderer = NULL;      // SDL 렌더러
TTF_Font* font = NULL;              // 폰트
SDL_Texture* fishTexture = NULL;    // 물고기 소스 텍스처

// 함수 프로토타입
bool engine_init();
void initGame();
void renderText(const char* text, int x, int y, SDL_Color color);
void renderFishTanks();
void updateGame();
void renderGame();
void cleanupGame();
void handleInput(SDL_Event* e);
SDL_Texture* loadTexture(const char* path);

int main(int argc, char* argv[]) {
    if (!engine_init()) {
        printf("엔진 초기화 실패: %s\n", SDL_GetError());
        return 1;
    }

    initGame();

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;
            handleInput(&event);
        }

        updateGame();
        renderGame();
        SDL_Delay(16); // 약 60 FPS 유지를 위해 16ms 대기로 변경 (반응성 향상)
    }

    cleanupGame();
    return 0;
}

bool engine_init() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return false;

    window = SDL_CreateWindow("Raising Fishes - SDL2 GUI", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    if (!window) return false;

    // 수직 동기화(VSYNC) 적용 렌더러 생성
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) return false;

    if (TTF_Init() != 0) return false;

    // Windows 기본 Arial 폰트 경로 사용
    font = TTF_OpenFont("C:\\Windows\\Fonts\\arial.ttf", 18);
    if (!font) {
        printf("폰트 로드 실패! 기본 경로를 확인하세요.\n");
        return false;
    }

    // 비트맵 이미지 로드 (실행 파일과 같은 폴더에 fish.bmp가 있어야 합니다)
    fishTexture = loadTexture("fish.bmp");
    if (!fishTexture) {
        printf("경고: fish.bmp를 찾을 수 없습니다. 임시 사각형 렌더링으로 대체합니다.\n");
        // 파일이 없어도 게임 구동이 가능하도록 에러 무시 처리 가능하나, 원본 흐름 유지
    }

    return true;
}

void initGame() {
    for (int i = 0; i < NUM; i++) {
        fishTanks[i].fish = 10.0f;
        fishTanks[i].water = 100.0f;
        fishTanks[i].isAlive = 1;
    }
    startTime = SDL_GetTicks();
    lastUpdateTime = startTime;
}

void renderGame() {
    // 배경: 어두운 밤바다 느낌의 짙은 네이비 색상
    SDL_SetRenderDrawColor(renderer, 10, 20, 40, 255);
    SDL_RenderClear(renderer);

    // 어항 및 물고기 렌더링
    renderFishTanks();

    // 상단 UI 정보 출력
    SDL_Color whiteText = { 255, 255, 255, 255 };
    char infoText[128];
    sprintf_s(infoText, sizeof(infoText), "LEVEL: %d   |   Time: %d sec", level, (SDL_GetTicks() - startTime) / 1000);
    renderText(infoText, 20, 20, whiteText);

    char helpText[] = "Controls: Move Left(J) / Move Right(L) / Give Water(K) / Exit(ESC)";
    renderText(helpText, 20, 50, whiteText);

    SDL_RenderPresent(renderer);
}

void renderFishTanks() {
    for (int i = 0; i < NUM; i++) {
        // 각 어항 배치 좌표 계산
        int x = 60 + i * (FISHTANK_WIDTH + 20);
        int y = 250;

        SDL_Rect bowl = { x, y, FISHTANK_WIDTH, FISHTANK_HEIGHT };

        // 1. 기본 어항 파란색 테두리 그리기
        SDL_SetRenderDrawColor(renderer, 0, 100, 255, 255);
        SDL_RenderDrawRect(renderer, &bowl);

        // 2. 어항 내부 채워진 물 그리기
        int waterHeight = (int)(fishTanks[i].water * FISHTANK_HEIGHT / 100.0f);
        SDL_Rect waterRect = { x + 2, y + FISHTANK_HEIGHT - waterHeight, FISHTANK_WIDTH - 4, waterHeight };

        // 물 수위에 따라 투명도나 색상 변화 (수위 낮으면 붉은 기운 추가)
        if (fishTanks[i].water < 30) {
            SDL_SetRenderDrawColor(renderer, 200, 50, 50, 200); // 경고: 빨간 물
        }
        else {
            SDL_SetRenderDrawColor(renderer, 0, 120, 220, 150); // 일반: 파란 물
        }
        SDL_RenderFillRect(renderer, &waterRect);

        // 3. 현재 커서(선택된 어항) 노란색 강조 테두리
        if (i == position) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
            // 두껍게 보이도록 2중 사각형 그리기
            SDL_RenderDrawRect(renderer, &bowl);
            SDL_Rect innerBowl = { bowl.x + 1, bowl.y + 1, bowl.w - 2, bowl.h - 2 };
            SDL_RenderDrawRect(renderer, &innerBowl);
        }

        // 4. ★ 상태별 물고기 이미지 및 그래픽 크기 조절 변경 렌더링
        if (fishTanks[i].isAlive) {
            int fishSizeW = 40;
            int fishSizeH = 20;

            // 상태에 따른 이미지 세팅 변조 (크기 및 색상 상태 변경)
            if (fishTanks[i].fish >= 100.0f) {
                // [상태 1: 황금 물고기] - 만렙 성장
                SDL_SetTextureColorMod(fishTexture, 255, 215, 0); // 황금색 변조
                fishSizeW = 70; fishSizeH = 35;
            }
            else if (fishTanks[i].fish > 40.0f) {
                // [상태 2: 어른 물고기] - 중간 성장
                SDL_SetTextureColorMod(fishTexture, 255, 255, 255); // 원래 색상
                fishSizeW = 60; fishSizeH = 30;
            }
            else {
                // [상태 3: 아기 물고기] - 초기 상태
                SDL_SetTextureColorMod(fishTexture, 150, 200, 255); // 하늘색빛 아기새끼
                fishSizeW = 40; fishSizeH = 20;
            }

            // 물고기가 물 한가운데에 위치하도록 좌표 연산 (물 높이에 연동)
            int fishX = x + (FISHTANK_WIDTH - fishSizeW) / 2;
            int fishY = y + FISHTANK_HEIGHT - (waterHeight / 2) - (fishSizeH / 2);

            // 안전 구역 제한 (어항 바닥을 뚫지 않게 제어)
            if (fishY > y + FISHTANK_HEIGHT - fishSizeH - 5) {
                fishY = y + FISHTANK_HEIGHT - fishSizeH - 5;
            }

            SDL_Rect fishRect = { fishX, fishY, fishSizeW, fishSizeH };

            if (fishTexture) {
                SDL_RenderCopy(renderer, fishTexture, NULL, &fishRect);
            }
            else {
                // 이미지가 없을 때 대용 사각형 그래픽
                SDL_RenderFillRect(renderer, &fishRect);
            }
        }
        else {
            // [상태 4: 유령/죽은 물고기] - 뒤집어진 유령 상태 표현
            SDL_SetTextureColorMod(fishTexture, 100, 100, 100); // 잿빛 색상 변조
            int fishX = x + (FISHTANK_WIDTH - 50) / 2;
            int fishY = y + FISHTANK_HEIGHT - 35; // 어항 바닥에 가라앉음
            SDL_Rect fishRect = { fishX, fishY, 50, 25 };

            if (fishTexture) {
                // SDL_RENDERFLIP_VERTICAL 속성을 사용해 물고기가 뒤집어져서 죽은 이미지 연출!
                SDL_RenderCopyEx(renderer, fishTexture, NULL, &fishRect, 0.0, NULL, SDL_FLIP_VERTICAL);
            }
        }

        // 5. 어항 하단 텍스트 출력 정보
        char status[64];
        SDL_Color textColor = { 255, 255, 255, 255 };

        if (fishTanks[i].isAlive) {
            sprintf_s(status, sizeof(status), "F:%d W:%d", (int)fishTanks[i].fish, (int)fishTanks[i].water);
        }
        else {
            sprintf_s(status, sizeof(status), "DEAD");
            textColor.r = 255; textColor.g = 50; textColor.b = 50; // 사망시 빨간 글씨
        }
        renderText(status, x + 5, y + FISHTANK_HEIGHT + 15, textColor);
    }
}

void updateGame() {
    unsigned int currentTime = SDL_GetTicks();
    // 초 단위 실수를 구해 미세한 프레임 시간차(dt) 누적 계산 반영
    float dt = (currentTime - lastUpdateTime) / 1000.0f;
    lastUpdateTime = currentTime;

    if (gameOver || gameWin) return;

    int aliveCount = 0;
    for (int i = 0; i < NUM; i++) {
        if (fishTanks[i].isAlive == 1) {
            // 1. 물 소비 및 증발 연산 (Level 및 물고기 크기에 비례)
            float waterDecreaseRate = level * (fishTanks[i].fish / 20.0f + 1.0f);
            fishTanks[i].water -= (waterDecreaseRate * dt * 2.5f); // 밸런스 조정 배율 추가

            if (fishTanks[i].water <= 0.0f) {
                fishTanks[i].water = 0.0f;
                fishTanks[i].isAlive = 0; // 사망 판정
            }

            // 2. 물고기 자동 성장 연산 (물이 가득 차 있을수록 빠르게 성장)
            if (fishTanks[i].water > 0.0f) {
                float growthRate = (fishTanks[i].water / 100.0f + 0.5f);
                fishTanks[i].fish += (growthRate * dt * 3.0f);
            }
            if (fishTanks[i].fish > 100.0f) fishTanks[i].fish = 100.0f;

            aliveCount++;
        }
    }

    // 모든 물고기 사망 시 패배 게임오버 트리거
    if (aliveCount == 0) {
        gameOver = true;
        running = false;
    }

    // 3. 레벨업 시스템 (시간 경과 기준)
    unsigned int totalElapsed = (currentTime - startTime) / 1000;
    if (totalElapsed / 20 > (unsigned int)(level - 1)) {
        level++;
        if (level > 5) {
            level = 5;
            gameWin = true;
            running = false;
        }
    }
}

void handleInput(SDL_Event* e) {
    if (e->type == SDL_KEYDOWN) {
        switch (e->key.keysym.sym) {
        case SDLK_j: // 왼쪽 어항 이동
            if (position > 0) position--;
            break;
        case SDLK_l: // 오른쪽 어항 이동
            if (position < NUM - 1) position++;
            break;
        case SDLK_k: // 선택 어항에 물주기
            if (fishTanks[position].isAlive && fishTanks[position].water < 100.0f) {
                fishTanks[position].water += 15.0f; // 물 공급량 상향 조정 (난이도 밸런스)
                if (fishTanks[position].water > 100.0f) fishTanks[position].water = 100.0f;
            }
            break;
        case SDLK_ESCAPE: // 게임 강제 종료
            running = false;
            break;
        }
    }
}

void renderText(const char* text, int x, int y, SDL_Color color) {
    if (!font) return;
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, color); // 가독성이 높은 Blended로 변경
    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_Rect dest = { x, y, surface->w, surface->h };
        SDL_RenderCopy(renderer, texture, NULL, &dest);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

SDL_Texture* loadTexture(const char* path) {
    SDL_Surface* surface = SDL_LoadBMP(path);
    if (!surface) {
        return NULL;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

void cleanupGame() {
    // 최종 결과 화면 연출 처리 부활
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_Color goldColor = { 255, 215, 0, 255 };
    SDL_Color redColor = { 255, 50, 50, 255 };

    if (gameWin) {
        renderText("YOU WIN! Max level achieved successfully!", 180, 260, goldColor);
    }
    else if (gameOver) {
        renderText("GAME OVER... All your fishes are dead.", 200, 260, redColor);
    }
    else {
        renderText("GAME CLOSED", 320, 260, redColor);
    }

    SDL_RenderPresent(renderer);
    SDL_Delay(3000); // 3초 대기 후 안전하게 정리 해제

    if (fishTexture) SDL_DestroyTexture(fishTexture);
    if (font) TTF_CloseFont(font);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);

    TTF_Quit();
    SDL_Quit();
}