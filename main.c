/*******************************************************************************************
*  LITERACY BATTLE - a flash-card game made with raylib (C)
*
*  Flow:   Mode (Math / English) -> Difficulty -> 10 rounds -> Result
*          Rounds 5 and 10 are boss fights (BOSS_CARDS flash cards each).
*          Correct answer = attack power goes up and you hit the enemy.
*          Wrong answer   = no power gain, move on.
*          After a normal round there is a chance of a one-time BONUS card
*          that gives a big attack boost.
*
*  Controls: mouse click, or keys 1-4 to pick an answer.
*
*  Build (Linux):
*      gcc main.c -o literacy_battle -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
*  Build (Windows / MinGW):
*      gcc main.c -o literacy_battle.exe -lraylib -lopengl32 -lgdi32 -lwinmm
*  Build (macOS):
*      clang main.c -o literacy_battle -lraylib -framework IOKit -framework Cocoa -framework OpenGL
*******************************************************************************************/

#include "raylib.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ============================== TUNING ============================== */
#define SCREEN_W        900
#define SCREEN_H        600

#define TOTAL_ROUNDS    10
#define BOSS_EVERY      5       /* rounds 5 and 10 are boss fights            */
#define BOSS_CARDS      3       /* flash cards per boss fight                 */
#define BASE_ATTACK     10
#define ATTACK_GAIN     5       /* attack power gained per correct answer     */
#define MINION_HP       10
#define BONUS_CHANCE    25      /* % chance after a normal round (max 1/run)  */
#define FEEDBACK_TIME   1.0f    /* seconds the right/wrong screen stays up    */
#define INTRO_TIME      1.2f    /* seconds the "ROUND X" screen stays up      */

#define NUM_CHOICES     4
#define DECK_MAX        32

/* Boss HP per difficulty: [difficulty][0 = round 5 boss, 1 = round 10 boss]  */
static const int BOSS_HP[3][2]      = { { 50, 90 }, { 75, 120 }, { 90, 160 } };
static const int BONUS_BOOST[3]     = { 10, 10, 15 };
static const char *DIFF_NAME[3]     = { "EASY", "MEDIUM", "HARD" };
static const char *SUBJECT_NAME[2]  = { "MATH", "ENGLISH" };
static const char *DIFF_DESC[2][3]  = {
    { "Single-digit + and -", "Two-digit math, times tables", "Division and mixed operations" },
    { "Spelling and word basics", "Synonyms and antonyms", "Grammar and word meanings" }
};

/* ============================== TYPES =============================== */
typedef enum { SUBJECT_MATH, SUBJECT_ENGLISH } Subject;
typedef enum { DIFF_EASY, DIFF_MEDIUM, DIFF_HARD } Difficulty;

typedef enum {
    SCREEN_MODE_SELECT,
    SCREEN_DIFFICULTY_SELECT,
    SCREEN_ROUND_INTRO,
    SCREEN_QUESTION,
    SCREEN_FEEDBACK,
    SCREEN_RESULT
} GameScreen;

typedef struct {
    char prompt[96];
    char choices[NUM_CHOICES][32];
    int  correct;                   /* index of the right choice */
} Question;

typedef struct {
    const char *prompt;
    const char *choices[NUM_CHOICES];
    int correct;
} EnglishCard;

typedef struct {
    GameScreen screen;
    Subject    subject;
    Difficulty difficulty;

    /* run progress */
    int  round;                     /* 1..TOTAL_ROUNDS */
    int  attack;
    int  correctCount;              /* main cards only (bonus not counted) */
    int  wrongCount;

    /* current enemy */
    int   enemyHp;
    int   enemyMaxHp;
    float shownHp;                  /* smoothed value for the HP bar */
    bool  isBoss;
    int   bossCard;                 /* 1..BOSS_CARDS */
    bool  isBonus;
    bool  bonusUsed;

    /* current card + feedback */
    Question q;
    int   picked;
    bool  lastCorrect;
    char  msg[128];
    char  msg2[128];
    float timer;

    /* shuffled English deck */
    int deck[DECK_MAX];
    int deckCount;
    int deckPos;

    bool won;
} Game;

/* ========================== ENGLISH CARD DATA ======================= */
static const EnglishCard ENGLISH_EASY[] = {
    { "Which word is spelled correctly?", { "freind", "friend", "frend", "firend" }, 1 },
    { "Which word is spelled correctly?", { "becuase", "becase", "because", "becaus" }, 2 },
    { "Which word is spelled correctly?", { "beautiful", "beutiful", "beatiful", "buetiful" }, 0 },
    { "Which word is spelled correctly?", { "wierd", "weird", "wiard", "wered" }, 1 },
    { "Which word is spelled correctly?", { "tomorow", "tomorrow", "tommorow", "tomoro" }, 1 },
    { "Which word is spelled correctly?", { "necessary", "neccessary", "necesary", "necessery" }, 0 },
    { "Which word is spelled correctly?", { "separate", "seperate", "separete", "seprate" }, 0 },
    { "Which word is spelled correctly?", { "definately", "definitely", "definitly", "defanitely" }, 1 },
    { "Which word is spelled correctly?", { "occured", "ocurred", "occurred", "occurrd" }, 2 },
    { "Which word is spelled correctly?", { "recieve", "receive", "receeve", "reseive" }, 1 },
    { "Which word is spelled correctly?", { "Wensday", "Wednsday", "Wedensday", "Wednesday" }, 3 },
    { "Which word is spelled correctly?", { "library", "libary", "liberry", "librery" }, 0 },
    { "Which word is spelled correctly?", { "February", "Febuary", "Febrary", "Feburary" }, 0 },
    { "She ___ to school every day.", { "go", "goes", "going", "gone" }, 1 },
    { "Which letter completes 'gr_en'?", { "e", "a", "i", "o" }, 0 },
    { "Which of these is a noun?", { "run", "happy", "table", "quickly" }, 2 },
};

static const EnglishCard ENGLISH_MEDIUM[] = {
    { "Which word means the SAME as 'happy'?",     { "sad", "joyful", "angry", "tired" }, 1 },
    { "Which word means the OPPOSITE of 'ancient'?", { "old", "modern", "huge", "fragile" }, 1 },
    { "Which word means the SAME as 'begin'?",     { "finish", "start", "pause", "stop" }, 1 },
    { "Which word means the OPPOSITE of 'generous'?", { "selfish", "kind", "wealthy", "polite" }, 0 },
    { "Which word means the SAME as 'enormous'?",  { "tiny", "huge", "narrow", "quiet" }, 1 },
    { "Which word means the OPPOSITE of 'victory'?", { "triumph", "defeat", "battle", "prize" }, 1 },
    { "Which word means the SAME as 'rapid'?",     { "slow", "fast", "late", "weak" }, 1 },
    { "Which word means the OPPOSITE of 'expand'?", { "grow", "stretch", "shrink", "build" }, 2 },
    { "Which word means the SAME as 'brave'?",     { "fearless", "nervous", "lazy", "clever" }, 0 },
    { "Which word means the OPPOSITE of 'difficult'?", { "hard", "easy", "tough", "complex" }, 1 },
    { "Which word means the SAME as 'silent'?",    { "loud", "quiet", "busy", "rough" }, 1 },
    { "Which word means the OPPOSITE of 'arrive'?", { "depart", "enter", "reach", "stay" }, 0 },
    { "Which word means the SAME as 'tired'?",     { "energetic", "weary", "cheerful", "hungry" }, 1 },
    { "Which word means the OPPOSITE of 'rough'?", { "smooth", "sharp", "heavy", "dry" }, 0 },
    { "Which word means the SAME as 'smart'?",     { "foolish", "clever", "slow", "shy" }, 1 },
    { "Which word means the OPPOSITE of 'always'?", { "often", "usually", "never", "sometimes" }, 2 },
};

static const EnglishCard ENGLISH_HARD[] = {
    { "___ going to the park after school.",       { "Their", "There", "They're", "Theyre" }, 2 },
    { "___ a beautiful day today.",                { "Its", "It's", "Its'", "Is" }, 1 },
    { "What does 'benevolent' mean?",              { "kind and generous", "cruel", "forgetful", "very loud" }, 0 },
    { "She is ___ than her sister.",               { "more taller", "taller", "tallest", "most tall" }, 1 },
    { "What does 'meticulous' mean?",              { "careless", "very careful", "very fast", "lonely" }, 1 },
    { "He ___ the test yesterday.",                { "pass", "passes", "passed", "passing" }, 2 },
    { "What does 'ambiguous' mean?",               { "unclear", "certain", "ancient", "cheerful" }, 0 },
    { "Neither of the boys ___ ready.",            { "are", "were", "is", "be" }, 2 },
    { "What does 'reluctant' mean?",               { "eager", "unwilling", "exhausted", "polite" }, 1 },
    { "I have ___ my homework already.",           { "do", "did", "done", "doing" }, 2 },
    { "What does 'scarce' mean?",                  { "plentiful", "rare", "expensive", "dangerous" }, 1 },
    { "The rain will ___ our plans.",              { "affect", "effect", "afect", "effects" }, 0 },
    { "What does 'frugal' mean?",                  { "wasteful", "careful with money", "very rich", "generous" }, 1 },
    { "To ___ should I address the letter?",       { "who", "whom", "whose", "which" }, 1 },
    { "What does 'inevitable' mean?",              { "impossible", "unavoidable", "surprising", "optional" }, 1 },
    { "Between you and ___, it's a secret.",       { "I", "me", "myself", "we" }, 1 },
};

static const EnglishCard *EnglishPool(Difficulty d, int *count)
{
    switch (d) {
        case DIFF_EASY:
            *count = (int)(sizeof(ENGLISH_EASY) / sizeof(ENGLISH_EASY[0]));
            return ENGLISH_EASY;
        case DIFF_MEDIUM:
            *count = (int)(sizeof(ENGLISH_MEDIUM) / sizeof(ENGLISH_MEDIUM[0]));
            return ENGLISH_MEDIUM;
        default:
            *count = (int)(sizeof(ENGLISH_HARD) / sizeof(ENGLISH_HARD[0]));
            return ENGLISH_HARD;
    }
}

/* =========================== QUESTION LOGIC ========================= */

/* Shuffle the 4 choices and keep q->correct pointing at the right one. */
static void ShuffleChoices(Question *q)
{
    for (int i = NUM_CHOICES - 1; i > 0; i--) {
        int j = GetRandomValue(0, i);
        char tmp[32];
        memcpy(tmp, q->choices[i], sizeof(tmp));
        memcpy(q->choices[i], q->choices[j], sizeof(tmp));
        memcpy(q->choices[j], tmp, sizeof(tmp));
        if (q->correct == i)      q->correct = j;
        else if (q->correct == j) q->correct = i;
    }
}

/* Fill choices with the right answer + 3 distinct nearby wrong answers. */
static void MakeNumberChoices(Question *q, int answer)
{
    int spread = answer / 5;
    if (spread < 4)  spread = 4;
    if (spread > 12) spread = 12;

    int wrong[3];
    int n = 0, guard = 0;
    while (n < 3 && guard++ < 200) {
        int off = GetRandomValue(1, spread) * (GetRandomValue(0, 1) ? 1 : -1);
        int w = answer + off;
        if (w < 0) continue;
        bool dup = (w == answer);
        for (int i = 0; i < n; i++) if (wrong[i] == w) dup = true;
        if (!dup) wrong[n++] = w;
    }
    while (n < 3) { wrong[n] = answer + spread + 1 + n; n++; }   /* safety net */

    snprintf(q->choices[0], sizeof(q->choices[0]), "%d", answer);
    for (int i = 0; i < 3; i++)
        snprintf(q->choices[i + 1], sizeof(q->choices[i + 1]), "%d", wrong[i]);
    q->correct = 0;
    ShuffleChoices(q);
}

static void GenerateMathQuestion(Question *q, Difficulty d)
{
    int a = 0, b = 0, c = 0, answer = 0;

    switch (d) {
        case DIFF_EASY:
            a = GetRandomValue(1, 9);
            b = GetRandomValue(1, 9);
            if (GetRandomValue(0, 1)) {
                answer = a + b;
                snprintf(q->prompt, sizeof(q->prompt), "%d + %d = ?", a, b);
            } else {
                if (b > a) { int t = a; a = b; b = t; }
                answer = a - b;
                snprintf(q->prompt, sizeof(q->prompt), "%d - %d = ?", a, b);
            }
            break;

        case DIFF_MEDIUM:
            if (GetRandomValue(0, 1)) {
                a = GetRandomValue(10, 99);
                b = GetRandomValue(10, 99);
                if (GetRandomValue(0, 1)) {
                    answer = a + b;
                    snprintf(q->prompt, sizeof(q->prompt), "%d + %d = ?", a, b);
                } else {
                    if (b > a) { int t = a; a = b; b = t; }
                    answer = a - b;
                    snprintf(q->prompt, sizeof(q->prompt), "%d - %d = ?", a, b);
                }
            } else {
                a = GetRandomValue(2, 12);
                b = GetRandomValue(2, 12);
                answer = a * b;
                snprintf(q->prompt, sizeof(q->prompt), "%d x %d = ?", a, b);
            }
            break;

        default: /* DIFF_HARD */
            switch (GetRandomValue(0, 2)) {
                case 0:   /* division with whole-number answer */
                    b = GetRandomValue(2, 12);
                    answer = GetRandomValue(2, 12);
                    a = b * answer;
                    snprintf(q->prompt, sizeof(q->prompt), "%d / %d = ?", a, b);
                    break;
                case 1:   /* order of operations */
                    a = GetRandomValue(2, 20);
                    b = GetRandomValue(2, 9);
                    c = GetRandomValue(2, 9);
                    answer = a + b * c;
                    snprintf(q->prompt, sizeof(q->prompt), "%d + %d x %d = ?", a, b, c);
                    break;
                default:  /* brackets */
                    a = GetRandomValue(2, 15);
                    b = GetRandomValue(2, 15);
                    c = GetRandomValue(2, 9);
                    answer = (a + b) * c;
                    snprintf(q->prompt, sizeof(q->prompt), "(%d + %d) x %d = ?", a, b, c);
                    break;
            }
            break;
    }
    MakeNumberChoices(q, answer);
}

static void BuildDeck(Game *g)
{
    int count;
    EnglishPool(g->difficulty, &count);
    if (count > DECK_MAX) count = DECK_MAX;

    g->deckCount = count;
    g->deckPos = 0;
    for (int i = 0; i < count; i++) g->deck[i] = i;
    for (int i = count - 1; i > 0; i--) {            /* Fisher-Yates */
        int j = GetRandomValue(0, i);
        int t = g->deck[i]; g->deck[i] = g->deck[j]; g->deck[j] = t;
    }
}

static void LoadQuestion(Game *g)
{
    if (g->subject == SUBJECT_MATH) {
        GenerateMathQuestion(&g->q, g->difficulty);
    } else {
        int count;
        const EnglishCard *pool = EnglishPool(g->difficulty, &count);
        if (g->deckPos >= g->deckCount) BuildDeck(g);   /* ran out: reshuffle */
        const EnglishCard *c = &pool[g->deck[g->deckPos++]];

        snprintf(g->q.prompt, sizeof(g->q.prompt), "%s", c->prompt);
        for (int i = 0; i < NUM_CHOICES; i++)
            snprintf(g->q.choices[i], sizeof(g->q.choices[i]), "%s", c->choices[i]);
        g->q.correct = c->correct;
        ShuffleChoices(&g->q);
    }
}

/* ============================ GAME FLOW ============================= */

static void BeginQuestion(Game *g)
{
    LoadQuestion(g);
    g->picked = -1;
    g->msg[0] = g->msg2[0] = '\0';
    g->screen = SCREEN_QUESTION;
}

static void StartRound(Game *g)
{
    g->isBoss   = (g->round % BOSS_EVERY == 0);
    g->isBonus  = false;
    g->bossCard = 1;

    if (g->isBoss) {
        int idx = g->round / BOSS_EVERY - 1;
        if (idx > 1) idx = 1;
        g->enemyMaxHp = BOSS_HP[g->difficulty][idx];
    } else {
        g->enemyMaxHp = MINION_HP;
    }
    g->enemyHp  = g->enemyMaxHp;
    g->shownHp  = (float)g->enemyHp;
    g->msg[0] = g->msg2[0] = '\0';
    g->screen = SCREEN_ROUND_INTRO;
    g->timer  = INTRO_TIME;
}

static void NextRound(Game *g)
{
    g->round++;
    StartRound(g);
}

static void StartRun(Game *g)
{
    g->round        = 1;
    g->attack       = BASE_ATTACK;
    g->correctCount = 0;
    g->wrongCount   = 0;
    g->bonusUsed    = false;
    g->won          = false;
    BuildDeck(g);
    StartRound(g);
}

static void StartBonus(Game *g)
{
    g->isBonus   = true;
    g->bonusUsed = true;
    BeginQuestion(g);
}

static void AnswerQuestion(Game *g, int idx)
{
    const char *ans = g->q.choices[g->q.correct];
    g->picked      = idx;
    g->lastCorrect = (idx == g->q.correct);
    g->msg2[0]     = '\0';

    if (g->isBonus) {
        if (g->lastCorrect) {
            int boost = BONUS_BOOST[g->difficulty];
            g->attack += boost;
            snprintf(g->msg, sizeof(g->msg), "BONUS! Attack power +%d  (now %d)", boost, g->attack);
        } else {
            snprintf(g->msg, sizeof(g->msg), "No bonus this time. The answer was: %s", ans);
        }
    } else {
        if (g->lastCorrect) {
            g->correctCount++;
            g->attack += ATTACK_GAIN;                       /* grow first, then hit */
            g->enemyHp -= g->attack;
            if (g->enemyHp < 0) g->enemyHp = 0;
            snprintf(g->msg, sizeof(g->msg), g->isBoss ? "Correct! You hit the boss for %d!"
                                                       : "Correct! You hit for %d!", g->attack);
            snprintf(g->msg2, sizeof(g->msg2), "Attack power is now %d", g->attack);
        } else {
            g->wrongCount++;
            snprintf(g->msg, sizeof(g->msg), "Wrong! The answer was: %s", ans);
            snprintf(g->msg2, sizeof(g->msg2), g->isBoss ? "The boss shrugs it off." : "On to the next round...");
        }

        if (g->isBoss && (g->enemyHp <= 0 || g->bossCard >= BOSS_CARDS)) {
            if (g->enemyHp <= 0) snprintf(g->msg2, sizeof(g->msg2), "BOSS DEFEATED!");
            else snprintf(g->msg2, sizeof(g->msg2), "The boss survived with %d HP!", g->enemyHp);
        }
    }

    g->screen = SCREEN_FEEDBACK;
    g->timer  = FEEDBACK_TIME;
}

/* Called when the feedback screen finishes: decides what comes next. */
static void AfterFeedback(Game *g)
{
    if (g->isBonus) {                       /* bonus card done -> next round */
        g->isBonus = false;
        NextRound(g);
        return;
    }

    if (g->isBoss && g->enemyHp > 0 && g->bossCard < BOSS_CARDS) {   /* more boss cards */
        g->bossCard++;
        BeginQuestion(g);
        return;
    }

    /* the round is over */
    if (g->round >= TOTAL_ROUNDS) {
        g->won = (!g->isBoss) || (g->enemyHp <= 0);
        g->screen = SCREEN_RESULT;
        return;
    }

    if (!g->isBoss && !g->bonusUsed && GetRandomValue(1, 100) <= BONUS_CHANCE) {
        StartBonus(g);
        return;
    }
    NextRound(g);
}

/* ============================ UI HELPERS ============================ */
static const Color COL_BG      = { 24, 24, 40, 255 };
static const Color COL_PANEL   = { 40, 40, 70, 255 };
static const Color COL_BUTTON  = { 52, 73, 140, 255 };
static const Color COL_GOOD    = { 40, 160, 80, 255 };
static const Color COL_BAD     = { 190, 60, 60, 255 };
static const Color COL_BADTXT  = { 255, 120, 120, 255 };

static bool IsHovered(Rectangle r) { return CheckCollisionPointRec(GetMousePosition(), r); }
static bool IsClicked(Rectangle r) { return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && IsHovered(r); }

/* Centered text that shrinks until it fits in maxWidth. */
static void DrawTextCentered(const char *text, int cx, int y, int fontSize, int maxWidth, Color color)
{
    while (fontSize > 12 && MeasureText(text, fontSize) > maxWidth) fontSize--;
    DrawText(text, cx - MeasureText(text, fontSize) / 2, y, fontSize, color);
}

static void DrawButton(Rectangle r, const char *label, Color base, int fontSize)
{
    Color c = IsHovered(r) ? ColorBrightness(base, 0.25f) : base;
    DrawRectangleRounded(r, 0.2f, 8, c);
    DrawTextCentered(label, (int)(r.x + r.width / 2), (int)(r.y + r.height / 2 - fontSize / 2),
                     fontSize, (int)r.width - 20, WHITE);
}

/* Layout rectangles (shared by Update and Draw so clicks always match visuals) */
static Rectangle ModeRect(int i)   { return (Rectangle){ 150.0f + i * 320.0f, 240.0f, 280.0f, 140.0f }; }
static Rectangle DiffRect(int i)   { return (Rectangle){ 70.0f + i * 270.0f, 250.0f, 250.0f, 120.0f }; }
static Rectangle BackRect(void)    { return (Rectangle){ 350.0f, 450.0f, 200.0f, 50.0f }; }
static Rectangle ChoiceRect(int i) { return (Rectangle){ 40.0f + (i % 2) * 430.0f, 385.0f + (i / 2) * 95.0f, 400.0f, 80.0f }; }
static Rectangle ResultRect(int i) { return (Rectangle){ 200.0f + i * 270.0f, 470.0f, 230.0f, 60.0f }; }

/* ============================ MODE SELECT =========================== */
static void UpdateModeSelect(Game *g)
{
    for (int i = 0; i < 2; i++) {
        if (IsClicked(ModeRect(i))) {
            g->subject = (Subject)i;
            g->screen = SCREEN_DIFFICULTY_SELECT;
        }
    }
}

static void DrawModeSelect(const Game *g)
{
    (void)g;
    DrawTextCentered("LITERACY BATTLE", SCREEN_W / 2, 80, 60, 800, GOLD);
    DrawTextCentered("Choose your challenge", SCREEN_W / 2, 170, 26, 800, LIGHTGRAY);
    DrawButton(ModeRect(0), "MATH", COL_BUTTON, 40);
    DrawButton(ModeRect(1), "ENGLISH", COL_BUTTON, 40);
    DrawTextCentered("Answer cards to power up. Defeat the bosses!", SCREEN_W / 2, 480, 22, 800, GRAY);
}

/* ========================== DIFFICULTY SELECT ======================= */
static void UpdateDifficultySelect(Game *g)
{
    for (int i = 0; i < 3; i++) {
        if (IsClicked(DiffRect(i))) {
            g->difficulty = (Difficulty)i;
            StartRun(g);
            return;
        }
    }
    if (IsClicked(BackRect())) g->screen = SCREEN_MODE_SELECT;
}

static void DrawDifficultySelect(const Game *g)
{
    char title[64];
    snprintf(title, sizeof(title), "%s - choose difficulty", SUBJECT_NAME[g->subject]);
    DrawTextCentered(title, SCREEN_W / 2, 100, 44, 800, GOLD);

    Color cols[3] = { COL_GOOD, { 200, 140, 30, 255 }, COL_BAD };
    for (int i = 0; i < 3; i++) {
        Rectangle r = DiffRect(i);
        DrawButton(r, DIFF_NAME[i], cols[i], 32);
        DrawTextCentered(DIFF_DESC[g->subject][i], (int)(r.x + r.width / 2), (int)(r.y + r.height + 12),
                         18, (int)r.width, LIGHTGRAY);
    }
    DrawButton(BackRect(), "BACK", (Color){ 70, 70, 90, 255 }, 24);
}

/* ============================ SHARED HUD ============================ */
static void DrawHeader(const Game *g)
{
    DrawRectangle(0, 0, SCREEN_W, 44, (Color){ 16, 16, 28, 255 });

    char buf[64];
    if (g->isBonus) {
        DrawText("BONUS ROUND", 20, 12, 22, GOLD);
    } else {
        snprintf(buf, sizeof(buf), "ROUND %d / %d", g->round, TOTAL_ROUNDS);
        DrawText(buf, 20, 12, 22, WHITE);
    }

    snprintf(buf, sizeof(buf), "%s - %s", SUBJECT_NAME[g->subject], DIFF_NAME[g->difficulty]);
    DrawTextCentered(buf, SCREEN_W / 2, 12, 22, 300, LIGHTGRAY);

    snprintf(buf, sizeof(buf), "ATK %d", g->attack);
    DrawText(buf, SCREEN_W - 20 - MeasureText(buf, 22), 12, 22, ORANGE);
}

static void DrawEnemy(const Game *g)
{
    float shake = 0.0f;
    if (g->screen == SCREEN_FEEDBACK && g->lastCorrect && !g->isBonus)
        shake = sinf((float)GetTime() * 60.0f) * 8.0f * (g->timer / FEEDBACK_TIME);
    int cx = SCREEN_W / 2 + (int)shake;

    if (g->isBonus) {                                   /* bonus: a golden chest-ish block */
        DrawRectangleRounded((Rectangle){ cx - 70.0f, 65.0f, 140.0f, 90.0f }, 0.3f, 8, GOLD);
        DrawTextCentered("BONUS", cx, 92, 34, 120, DARKBROWN);
        DrawTextCentered("Answer to power up!", SCREEN_W / 2, 170, 22, 400, GOLD);
        return;
    }

    bool dead = (g->enemyHp <= 0);
    float w = g->isBoss ? 220.0f : 130.0f;
    float h = g->isBoss ? 105.0f : 90.0f;
    Color body = dead ? DARKGRAY : (g->isBoss ? MAROON : DARKGREEN);
    Rectangle r = { cx - w / 2, 155.0f - h, w, h };

    DrawRectangleRounded(r, 0.25f, 8, body);
    if (dead) {
        DrawText("x", (int)(cx - w * 0.22f) - 6, (int)(r.y + h * 0.25f), 28, WHITE);
        DrawText("x", (int)(cx + w * 0.22f) - 6, (int)(r.y + h * 0.25f), 28, WHITE);
    } else {
        DrawCircle((int)(cx - w * 0.22f), (int)(r.y + h * 0.38f), h * 0.12f, WHITE);
        DrawCircle((int)(cx + w * 0.22f), (int)(r.y + h * 0.38f), h * 0.12f, WHITE);
        DrawCircle((int)(cx - w * 0.22f), (int)(r.y + h * 0.38f), h * 0.05f, BLACK);
        DrawCircle((int)(cx + w * 0.22f), (int)(r.y + h * 0.38f), h * 0.05f, BLACK);
    }
    DrawTextCentered(g->isBoss ? "BOSS" : "MINION", cx, (int)(r.y + h - 30), 20, (int)w - 10, WHITE);

    /* HP bar */
    Rectangle bar = { 250.0f, 168.0f, 400.0f, 22.0f };
    float pct = (g->enemyMaxHp > 0) ? g->shownHp / (float)g->enemyMaxHp : 0.0f;
    if (pct < 0.0f) pct = 0.0f;
    DrawRectangleRec(bar, (Color){ 60, 20, 20, 255 });
    DrawRectangle((int)bar.x, (int)bar.y, (int)(bar.width * pct), (int)bar.height, RED);
    DrawRectangleLinesEx(bar, 2, WHITE);
    char hp[32];
    snprintf(hp, sizeof(hp), "HP %d / %d", g->enemyHp, g->enemyMaxHp);
    DrawTextCentered(hp, SCREEN_W / 2, (int)bar.y + 3, 16, 380, WHITE);

    if (g->isBoss) {
        char card[32];
        snprintf(card, sizeof(card), "BOSS CARD %d / %d", g->bossCard, BOSS_CARDS);
        DrawTextCentered(card, SCREEN_W / 2, 198, 18, 300, GOLD);
    }
}

static void DrawQuestionBody(const Game *g, bool reveal)
{
    DrawRectangleRounded((Rectangle){ 40.0f, 235.0f, 820.0f, 70.0f }, 0.2f, 8, COL_PANEL);
    DrawTextCentered(g->q.prompt, SCREEN_W / 2, 254, 32, 780, WHITE);

    if (reveal) {
        DrawTextCentered(g->msg, SCREEN_W / 2, 314, 22, 820, g->lastCorrect ? LIME : COL_BADTXT);
        if (g->msg2[0]) DrawTextCentered(g->msg2, SCREEN_W / 2, 344, 20, 820, GOLD);
    }

    for (int i = 0; i < NUM_CHOICES; i++) {
        Rectangle r = ChoiceRect(i);
        Color base = COL_BUTTON;
        if (!reveal) {
            if (IsHovered(r)) base = ColorBrightness(base, 0.25f);
        } else if (i == g->q.correct) {
            base = COL_GOOD;
        } else if (i == g->picked) {
            base = COL_BAD;
        } else {
            base = (Color){ 50, 50, 70, 255 };
        }
        DrawRectangleRounded(r, 0.2f, 8, base);

        char label[64];
        snprintf(label, sizeof(label), "%d. %s", i + 1, g->q.choices[i]);
        DrawTextCentered(label, (int)(r.x + r.width / 2), (int)(r.y + r.height / 2 - 13), 26, (int)r.width - 30, WHITE);
    }
}

/* ============================ ROUND INTRO =========================== */
static void UpdateRoundIntro(Game *g, float dt)
{
    g->timer -= dt;
    if (g->timer <= 0.0f || IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsKeyPressed(KEY_SPACE))
        BeginQuestion(g);
}

static void DrawRoundIntro(const Game *g)
{
    DrawHeader(g);
    DrawEnemy(g);

    char buf[96];
    if (g->isBoss) {
        DrawTextCentered("BOSS FIGHT!", SCREEN_W / 2, 280, 64, 800, (Color){ 255, 90, 90, 255 });
        snprintf(buf, sizeof(buf), "%d cards - every correct answer hits harder", BOSS_CARDS);
    } else {
        snprintf(buf, sizeof(buf), "ROUND %d", g->round);
        DrawTextCentered(buf, SCREEN_W / 2, 280, 64, 800, WHITE);
        snprintf(buf, sizeof(buf), "Get ready!");
    }
    DrawTextCentered(buf, SCREEN_W / 2, 370, 26, 800, LIGHTGRAY);
    DrawTextCentered("Click to continue", SCREEN_W / 2, 560, 18, 400, GRAY);
}

/* ============================== QUESTION ============================ */
static void UpdateQuestion(Game *g)
{
    for (int i = 0; i < NUM_CHOICES; i++) {
        if (IsClicked(ChoiceRect(i)) || IsKeyPressed(KEY_ONE + i)) {
            AnswerQuestion(g, i);
            return;
        }
    }
}

static void DrawQuestion(const Game *g)
{
    DrawHeader(g);
    DrawEnemy(g);
    DrawQuestionBody(g, false);
}

/* ============================== FEEDBACK ============================ */
static void UpdateFeedback(Game *g, float dt)
{
    g->timer -= dt;
    if (g->timer <= 0.0f || IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsKeyPressed(KEY_SPACE))
        AfterFeedback(g);
}

static void DrawFeedback(const Game *g)
{
    DrawHeader(g);
    DrawEnemy(g);
    DrawQuestionBody(g, true);
}

/* ============================== RESULT ============================== */
static void UpdateResult(Game *g)
{
    if (IsClicked(ResultRect(0))) StartRun(g);                    /* same mode + difficulty */
    if (IsClicked(ResultRect(1))) g->screen = SCREEN_MODE_SELECT;
}

static void DrawResult(const Game *g)
{
    char buf[96];
    DrawTextCentered(g->won ? "VICTORY!" : "DEFEAT", SCREEN_W / 2, 50, 72, 800, g->won ? GOLD : COL_BADTXT);

    if (g->won) snprintf(buf, sizeof(buf), "The final boss has been defeated!");
    else        snprintf(buf, sizeof(buf), "The final boss survived with %d HP", g->enemyHp);
    DrawTextCentered(buf, SCREEN_W / 2, 140, 24, 800, LIGHTGRAY);

    int total = g->correctCount + g->wrongCount;
    int pct = (total > 0) ? (g->correctCount * 100) / total : 0;

    snprintf(buf, sizeof(buf), "%s - %s", SUBJECT_NAME[g->subject], DIFF_NAME[g->difficulty]);
    DrawTextCentered(buf, SCREEN_W / 2, 215, 28, 800, WHITE);
    snprintf(buf, sizeof(buf), "Correct answers: %d / %d", g->correctCount, total);
    DrawTextCentered(buf, SCREEN_W / 2, 265, 28, 800, WHITE);
    snprintf(buf, sizeof(buf), "Accuracy: %d%%", pct);
    DrawTextCentered(buf, SCREEN_W / 2, 315, 28, 800, WHITE);
    snprintf(buf, sizeof(buf), "Final attack power: %d", g->attack);
    DrawTextCentered(buf, SCREEN_W / 2, 365, 28, 800, ORANGE);

    DrawButton(ResultRect(0), "PLAY AGAIN", COL_GOOD, 26);
    DrawButton(ResultRect(1), "MAIN MENU", COL_BUTTON, 26);
}

/* ================================ MAIN ============================== */
int main(void)
{
    InitWindow(SCREEN_W, SCREEN_H, "Literacy Battle");
    SetTargetFPS(60);
    SetRandomSeed((unsigned int)time(NULL));

    Game g;
    memset(&g, 0, sizeof(g));
    g.screen = SCREEN_MODE_SELECT;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        /* ---- update ---- */
        switch (g.screen) {
            case SCREEN_MODE_SELECT:       UpdateModeSelect(&g);       break;
            case SCREEN_DIFFICULTY_SELECT: UpdateDifficultySelect(&g); break;
            case SCREEN_ROUND_INTRO:       UpdateRoundIntro(&g, dt);   break;
            case SCREEN_QUESTION:          UpdateQuestion(&g);         break;
            case SCREEN_FEEDBACK:          UpdateFeedback(&g, dt);     break;
            case SCREEN_RESULT:            UpdateResult(&g);           break;
        }

        /* smooth HP bar toward the real value */
        float diff = (float)g.enemyHp - g.shownHp;
        g.shownHp += diff * fminf(1.0f, 8.0f * dt);
        if (fabsf(diff) < 0.5f) g.shownHp = (float)g.enemyHp;

        /* ---- draw ---- */
        BeginDrawing();
        ClearBackground(COL_BG);
        switch (g.screen) {
            case SCREEN_MODE_SELECT:       DrawModeSelect(&g);       break;
            case SCREEN_DIFFICULTY_SELECT: DrawDifficultySelect(&g); break;
            case SCREEN_ROUND_INTRO:       DrawRoundIntro(&g);       break;
            case SCREEN_QUESTION:          DrawQuestion(&g);         break;
            case SCREEN_FEEDBACK:          DrawFeedback(&g);         break;
            case SCREEN_RESULT:            DrawResult(&g);           break;
        }
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
