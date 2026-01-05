#include <cmath>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <string>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <functional>
#include <iostream>
#include <fstream>

const int WINDOW_WIDTH = 1500;
const int WINDOW_HEIGHT = 800;
const float PLAYER_SPEED = 300.0f;

// Bullet

const float P_BULLET_SPEED = 520.0f;
const float E_BULLET_SPEED = 240.0f;

const int MAX_P_BULLETS = 64;
const int MAX_E_BULLETS = 64;
const int MAX_ITEMS = 16;
const int MAX_ENEMIES = 36;

// Team/formation move
const float FORM_SPEED = 90.f;
const float FORM_MARGIN = 60.0f;
const float FORM_DROP_Y = 24.f;

// Formation layout
const int FORM_ROWS = 3;
const int FORM_COLS = 6;
const float FORM_START_X = 120.f;
const float FORM_START_Y = 110.f;
const float FORM_GAP_X = 110.f;
const float FORM_GAP_Y = 70.f;

// MISC
const float ENEMY_FIRE_BASE_COOLDOWN = 2.5f;
const float ITEM_FALL_SPEED = 120.0f;
const int DROP_CHANCE_PERCENT = 36;

// EXPLOSION EFFECT
const int EXPLOSION_FRAMES = 16;
const int EXPLOSION_FRAMES_PER_ROW = 4;
const float EXPLOSION_FRAME_DURATION = 0.05f;
const int MAX_EXPLOSIONS = 32;

// PICKUP EFFECT
const int MAX_PICKUP_EFFECTS = 16;
const float EFFECT_EXPAND_SPEED = 200.f;
const float EFFECT_FADE_SPEED = 500.f;

// SOUND
sf::SoundBuffer shootBuffer;
sf::SoundBuffer explosionBuffer;
sf::SoundBuffer deathBuffer;
sf::SoundBuffer hitBuffer;
sf::SoundBuffer crashBuffer;
sf::SoundBuffer powerupBuffer;

sf::Sound shootSound;
sf::Sound explosionSound;
sf::Sound deathSound;
sf::Sound hitSound;
sf::Sound crashSound;
sf::Sound powerupSound;

sf::Music menuMusic;
sf::Music gameMusic;

struct decoEnemy
{
    sf::Vector2f pos;
    sf::Vector2f vel;
    float radius;
};

struct Button
{
    sf::RectangleShape rect;
    sf::Text text;
    bool isHovered(const sf::Vector2f &mousePos) const
    {
        return rect.getGlobalBounds().contains(mousePos);
    }
    float t = 0;
};

enum GameMode
{
    MODE_NORMAL,
    MODE_HARD,
    MODE_SURVIVAL
};
GameMode currentMode = MODE_NORMAL;

enum GameState
{
    MENU,
    PLAYING,
    GAME_OVER,
    PAUSED,
    SAVE_SLOTS,
    HELP,
    HIGH_SCORES,
    MODE
};

enum ShootingStyle
{
    SINGLE,
    DOUBLE,
    SPREAD
};

ShootingStyle style = SINGLE;

enum ItemType
{
    ITEM_DMG,
    ITEM_SINGLE,
    ITEM_DOUBLE,
    ITEM_SPREAD,
    HEAL
};

static std::string toBinary(int value)
{
    int width = (value <= 15 ? 4 : 6);
    std::string s;
    s.reserve(width);
    for (int i = width - 1; i >= 0; i--)
    {
        s.push_back(((value >> i) & 1) ? '1' : '0');
    }
    return s;
}

static float dist2(const sf::Vector2f &a, const sf::Vector2f &b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

static bool circleHit(const sf::Vector2f &a, float ra, const sf::Vector2f &b, float rb)
{
    float r = ra + rb;
    return dist2(a, b) <= r * r;
}

struct Player
{
    sf::Vector2f pos{WINDOW_WIDTH / 2.f, WINDOW_HEIGHT - 70.0f};
    float radius = 20.f;
    int hp = 100;
    int damage = 20;
};

struct Enemy
{
    bool active = false;
    bool boss = false;
    int bossType = 0;
    int phase = 1;
    bool attackMode = false;
    bool returning = false;
    sf::Vector2f pos;
    sf::Vector2f basePos;
    float radius = 26.f;
    float t = 0.f;
    float fireCD = 1.0f;
    int hp = 10;
    int maxHp = 100;
    int gridX = -1, gridY = -1;
    float hitTimer = 0.f;
    float attackTimer = 0.f;
};

struct BulletText
{
    bool active = false;
    sf::Vector2f vel;
    sf::Text text;
    int damage = 1;
    sf::Vector2f pos;
    float radius = 14.f;
};

struct Item
{
    bool active = false;
    sf::Vector2f pos;
    sf::Text text;
    float radius = 16.f;
    ItemType type;
};

struct Explosion
{
    bool active = false;
    sf::Vector2f pos;
    float frameTimer = 0.f;
    int currentFrame = 0;
};

struct PickupEffect
{
    bool active = false;
    sf::Vector2f pos;
    float radius = 0.f;
    float alpha = 255.f; // Transparent
    sf::Color color;
};

void saveGame(const Player &player, int level, int score, ShootingStyle style,
              const Enemy enemies[], const Item items[], int slot)
{
    std::ofstream out("Save" + std::to_string(slot) + ".txt");
    if (!out.is_open())
        return;

    // Header
    out << "LEVEL " << level << "\n";
    out << "SCORE " << score << "\n";

    // Player: hp damage pos.x pos.y style
    out << "PLAYER " << player.hp << " " << player.damage << " "
        << player.pos.x << " " << player.pos.y << " " << (int)style << "\n";

    // Enemies
    out << "ENEMIES " << MAX_ENEMIES << "\n";
    for (int i = 0; i < MAX_ENEMIES; ++i)
    {
        const Enemy &e = enemies[i];
        out << (e.active ? 1 : 0) << " " << (e.boss ? 1 : 0) << " " << e.bossType << " " << e.phase << " " << e.hp << " " << e.basePos.x << " " << e.basePos.y << " " << (e.attackMode ? 1 : 0) << " " << (e.returning ? 1 : 0) << " " << e.fireCD << "\n";
    }
    // Items
    out << "ITEMS " << MAX_ITEMS << "\n";
    for (int i = 0; i < MAX_ITEMS; ++i)
    {
        const Item &it = items[i];
        out << (it.active ? 1 : 0) << " ";
        int t = (it.active ? (int)it.type : -1);
        out << t << " " << it.pos.x << " " << it.pos.y << "\n";
    }

    out.close();
}

bool loadGame(Player &player, int &level, int &score, ShootingStyle &style,
              Enemy enemies[], Item items[], BulletText pBullets[], BulletText eBullets[], int slot)
{
    std::ifstream in("Save" + std::to_string(slot) + ".txt");
    if (!in.is_open())
        return false;

    std::string tag;
    // LEVEL
    in >> tag;
    if (tag != "LEVEL")
        return false;
    in >> level;

    // SCORE
    in >> tag;
    if (tag != "SCORE")
        return false;
    in >> score;

    // PLAYER
    in >> tag;
    if (tag != "PLAYER")
        return false;
    int styleInt;
    in >> player.hp >> player.damage >> player.pos.x >> player.pos.y >> styleInt;
    style = (ShootingStyle)styleInt;

    // ENEMIES
    int enemyCount = 0;
    in >> tag;
    if (tag != "ENEMIES")
        return false;
    in >> enemyCount;

    enemyCount = std::min(enemyCount, MAX_ENEMIES);
    for (int i = 0; i < MAX_ENEMIES; ++i)
    {
        if (i < enemyCount)
        {
            Enemy &e = enemies[i];
            int activeInt, bossInt;
            in >> activeInt >> bossInt >> e.bossType >> e.phase >> e.hp >> e.pos.x >> e.pos.y;
            int attackModeInt, returningInt;
            in >> attackModeInt >> returningInt >> e.fireCD;
            e.active = (activeInt != 0);
            e.boss = (bossInt != 0);
            e.attackMode = (attackModeInt != 0);
            e.returning = (returningInt != 0);
            e.basePos = e.pos;
            e.t = 0.f;

            if (e.boss)
                e.radius = 50.f;
            else
                e.radius = 26.f;
            if (e.hp <= 0)
                e.active = false;
        }
        else
        {
            // read
            int a, b, c, d;
            float fx, fy, fcd;
            in >> a >> b >> c >> d >> a >> fx >> fy >> a >> b >> fcd;
        }
    }

    // ITEMS
    int itemCount = 0;
    in >> tag;
    if (tag != "ITEMS")
        return false;
    in >> itemCount;
    itemCount = std::min(itemCount, MAX_ITEMS);
    for (int i = 0; i < MAX_ITEMS; ++i)
    {
        if (i < itemCount)
        {
            Item &it = items[i];
            int activeInt, typeInt;
            in >> activeInt >> typeInt >> it.pos.x >> it.pos.y;
            it.active = (activeInt != 0);
            if (it.active && typeInt >= 0)
            {
                it.type = (ItemType)typeInt;
                it.text.setFont(player.hp >= 0 ? sf::Font() : sf::Font());
                it.text.setPosition(it.pos);
            }
            else
            {
                it.active = false;
            }
        }
        else
        {

            int a;
            float fx, fy;
            in >> a >> a >> fx >> fy;
        }
    }

    in.close();

    for (int i = 0; i < MAX_P_BULLETS; ++i)
        pBullets[i].active = false;
    for (int i = 0; i < MAX_E_BULLETS; ++i)
        eBullets[i].active = false;

    return true;
}
// High Scores management
struct HighScoreEntry
{
    int score;
    int level;
};

void loadHighScores(std::map<GameMode, std::vector<HighScoreEntry>> &allScores)
{
    allScores.clear();
    std::ifstream in("HighScores.txt");
    if (!in.is_open())
        return;

    std::string tag;
    while (in >> tag)
    {
        if (tag == "MODE")
        {
            int m;
            in >> m;
            int n;
            in >> n;
            std::vector<HighScoreEntry> entries;
            for (int i = 0; i < n; i++)
            {
                HighScoreEntry e;
                in >> e.score >> e.level;
                entries.push_back(e);
            }
            allScores[(GameMode)m] = entries;
        }
    }
}

void saveHighScores(const std::map<GameMode, std::vector<HighScoreEntry>> &allScores)
{
    std::ofstream out("HighScores.txt");
    if (!out.is_open())
        return;
    for (auto &kv : allScores)
    {
        GameMode mode = kv.first;
        const auto &scores = kv.second;
        out << "MODE " << (int)mode << "\n";
        out << scores.size() << "\n";
        for (auto &e : scores)
        {
            out << e.score << " " << e.level << "\n";
        }
    }
}

void addHighScore(int score, int level, GameMode mode)
{
    std::map<GameMode, std::vector<HighScoreEntry>> allScores;
    loadHighScores(allScores);

    auto &scores = allScores[mode];
    scores.push_back({score, level});

    // Sắp xếp giảm dần theo score
    std::sort(scores.begin(), scores.end(), [](auto &a, auto &b)
              { return a.score > b.score; });

    if (scores.size() > 5)
        scores.resize(5);

    saveHighScores(allScores);
}

// Survival mode
float survivalTimer = 0.f;
float enemySpawnCD = 2.f;
int lastBossSpawn = -1;

void resetGame(Player &player, int &level, int &score, BulletText pBullets[], BulletText eBullets[], Item items[], std::function<void(int)> spawnFunc, ShootingStyle &style)
{
    player.hp = 100;
    player.damage = 2;
    player.pos = {WINDOW_WIDTH / 2.f, WINDOW_HEIGHT - 70.0f}; // Reset vị trí
    level = 1;
    score = 0;
    style = SINGLE;

    spawnFunc(level);

    // Clear
    for (int i = 0; i < MAX_E_BULLETS; ++i)
        eBullets[i].active = false;
    for (int i = 0; i < MAX_P_BULLETS; ++i)
        pBullets[i].active = false;
    for (int i = 0; i < MAX_ITEMS; ++i)
        items[i].active = false;
    if (currentMode == MODE_SURVIVAL)
    {
        survivalTimer = 0.f;
        enemySpawnCD = 2.f;
        lastBossSpawn = -1;
    }
}

int main()
{

    std::srand(static_cast<unsigned>(std::time(nullptr)));
    sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "Numeric Invasion");
    window.setVerticalSyncEnabled(true);

    struct Star
    {
        sf::Vector2f pos;
        float speed;
        float radius;
        sf::Color color;
    };

    std::vector<Star> stars;
    const int STAR_COUNT = 150;

    for (int i = 0; i < STAR_COUNT; i++)
    {
        Star s;
        s.pos = {static_cast<float>(std::rand() % WINDOW_WIDTH), (static_cast<float>(std::rand() % WINDOW_HEIGHT))};
        s.speed = 20.f + static_cast<float>(std::rand() % 80);
        s.radius = (std::rand() % 2 == 0 ? 1.5f : 2.5f);
        s.color = (std::rand() % 4 == 0 ? sf::Color(200, 200, 255) : sf::Color::White);
        stars.push_back(s);
    }

    std::vector<decoEnemy> decoEnemies;
    for (int i = 0; i < 5; i++)
    {
        decoEnemy e;
        e.pos = {static_cast<float>(std::rand() % WINDOW_WIDTH),
                 static_cast<float>(50 + std::rand() % 300)};
        e.vel = {(std::rand() % 2 == 0 ? 60.f : -60.f), 0.f};
        e.radius = 20.f;
        decoEnemies.push_back(e);
    }

    // Font
    sf::Font font;
    if (!font.loadFromFile("Pixel Game.otf"))
    {
        return -1;
    }
    sf::Texture texPlayer, texEnemy, texBoss, texHitEnemy;
    if (!texPlayer.loadFromFile("Player2.png"))
        return -1;
    if (!texEnemy.loadFromFile("Enemy1.png"))
        return -1;
    if (!texBoss.loadFromFile("Boss1.png"))
        return -1;
    if (!texHitEnemy.loadFromFile("Player2.png"))
        return -1;

    sf::Texture texExplosion;
    if (!texExplosion.loadFromFile("explosion.png"))
        return -1;

    // Load sound effects
    if (!shootBuffer.loadFromFile("shoot.wav"))
        std::cout << "Failed to load shoot.wav\n";
    if (!explosionBuffer.loadFromFile("explosion.wav"))
        std::cout << "Failed to load explosion.wav\n";
    if (!deathBuffer.loadFromFile("death.wav"))
        std::cout << "Failed to load death.wav\n";
    if (!hitBuffer.loadFromFile("click_x.wav"))
        std::cout << "Failed to load hit.wav\n";
    if (!crashBuffer.loadFromFile("crash_x.wav"))
        std::cout << "Failed to load crash.wav\n";
    if (!powerupBuffer.loadFromFile("Powerup.wav"))
        std::cout << "Failed to load Powerup.wav\n";

    // Assign to sounds
    shootSound.setBuffer(shootBuffer);
    explosionSound.setBuffer(explosionBuffer);
    deathSound.setBuffer(deathBuffer);
    hitSound.setBuffer(hitBuffer);
    crashSound.setBuffer(crashBuffer);
    powerupSound.setBuffer(powerupBuffer);

    shootSound.setVolume(40.f);
    powerupSound.setVolume(50.f);
    crashSound.setVolume(50.f);
    menuMusic.setVolume(60.f);

    // Load music
    if (!menuMusic.openFromFile("Menu.mp3"))
        std::cout << "Failed to load menu.mp3\n";
    if (!gameMusic.openFromFile("In_game.mp3"))
        std::cout << "Failed to load game.mp3\n";

    // Set looping
    menuMusic.setLoop(true);
    gameMusic.setLoop(true);

    // Play menu music at start
    menuMusic.play();

    Button startButton;
    Button modeButton;
    Button helpButton;
    Button saveSlotsButton;
    Button highScoresButton;
    Button slot1, slot2, slot3, backButton;
    Button normalModeButton, hardModeButton, survivalModeButton, modeBackButton;

    auto setupButton = [&](Button &btn, const std::string &label, const sf::Vector2f &pos)
    {
        btn.rect.setSize({200, 50});
        btn.rect.setOrigin(100, 25);
        btn.rect.setPosition(pos);
        btn.rect.setFillColor(sf::Color(50, 50, 50));
        btn.rect.setOutlineColor(sf::Color::White);
        btn.rect.setOutlineThickness(2);

        btn.text.setFont(font);
        btn.text.setString(label);
        btn.text.setCharacterSize(24);
        btn.text.setFillColor(sf::Color::White);
        sf::FloatRect textBounds = btn.text.getLocalBounds();
        btn.text.setOrigin(textBounds.left + textBounds.width / 2.0f, textBounds.top + textBounds.height / 2.0f);
        btn.text.setPosition(pos);
    };
    const float BUTTON_SPACING = 70.f;
    const float SAVESLOT_BUTTON_SPACING = 90.f;
    const float modeButtonSpacing = 120.f;
    float currentY = WINDOW_HEIGHT / 2.0f - BUTTON_SPACING * 2;
    float saveSlotY = WINDOW_HEIGHT / 2.0f - SAVESLOT_BUTTON_SPACING;
    float modeY = WINDOW_HEIGHT / 2.0f - modeButtonSpacing;

    setupButton(startButton, "Start", {WINDOW_WIDTH / 2.0f, currentY});
    currentY += BUTTON_SPACING;
    setupButton(modeButton, "Mode", {WINDOW_WIDTH / 2.0f, currentY});
    currentY += BUTTON_SPACING;
    setupButton(helpButton, "Help", {WINDOW_WIDTH / 2.0f, currentY});
    currentY += BUTTON_SPACING;
    setupButton(saveSlotsButton, "Save Slots", {WINDOW_WIDTH / 2.0f, currentY});
    currentY += BUTTON_SPACING;
    setupButton(highScoresButton, "Hall of Fame", {WINDOW_WIDTH / 2.0f, currentY});
    setupButton(slot1, "Slot 1", {WINDOW_WIDTH / 2.f, saveSlotY});
    saveSlotY += SAVESLOT_BUTTON_SPACING;
    setupButton(slot2, "Slot 2", {WINDOW_WIDTH / 2.f, saveSlotY});
    saveSlotY += SAVESLOT_BUTTON_SPACING;
    setupButton(slot3, "Slot 3", {WINDOW_WIDTH / 2.f, saveSlotY});
    saveSlotY += SAVESLOT_BUTTON_SPACING;
    setupButton(backButton, "Back", {WINDOW_WIDTH / 2.f, saveSlotY});
    setupButton(normalModeButton, "Normal", {WINDOW_WIDTH / 2.f, modeY});
    modeY += modeButtonSpacing;
    setupButton(hardModeButton, "Hard", {WINDOW_WIDTH / 2.f, modeY});
    modeY += modeButtonSpacing;
    setupButton(survivalModeButton, "Survival", {WINDOW_WIDTH / 2.f, modeY});
    modeY += modeButtonSpacing;
    setupButton(modeBackButton, "Back", {WINDOW_WIDTH / 2.f, modeY});

    const sf::Color NORMAL_BUTTON_COLOR(50, 50, 50);
    const sf::Color HOVER_BUTTON_COLOR(80, 80, 80);
    const sf::Color NORMAL_TEXT_COLOR(255, 255, 255);
    const sf::Color HOVER_TEXT_COLOR(255, 255, 0); // Vàng

    auto updateButtonAppearance = [&](Button &btn, const sf::Vector2f &mousePos)
    {
        if (btn.isHovered(mousePos))
        {
            btn.rect.setFillColor(HOVER_BUTTON_COLOR);
            btn.rect.setOutlineColor(sf::Color::Yellow);
            btn.text.setFillColor(HOVER_TEXT_COLOR);
            btn.rect.setScale(1.1f, 1.1f);
            btn.text.setScale(1.1f, 1.1f);
        }
        else
        {
            btn.rect.setFillColor(NORMAL_BUTTON_COLOR);
            btn.rect.setOutlineColor(sf::Color::White);
            btn.text.setFillColor(NORMAL_TEXT_COLOR);
            btn.rect.setScale(1.0f, 1.0f);
            btn.text.setScale(1.0f, 1.0f);
        }
    };

    sf::Text gameOverText;
    gameOverText.setFont(font);
    gameOverText.setString("GAME OVER");
    gameOverText.setCharacterSize(80);
    gameOverText.setFillColor(sf::Color::Red);
    sf::FloatRect goBounds = gameOverText.getLocalBounds();
    gameOverText.setOrigin(goBounds.left + goBounds.width / 2.f, goBounds.top + goBounds.height / 2.f);
    gameOverText.setPosition(WINDOW_WIDTH / 2.f, WINDOW_HEIGHT / 2.f - 150.f);

    sf::Text finalScoreText;
    finalScoreText.setFont(font);
    finalScoreText.setCharacterSize(40);
    finalScoreText.setFillColor(sf::Color::White);

    Button restartButton;
    setupButton(restartButton, "Play Again", {WINDOW_WIDTH / 2.f, WINDOW_HEIGHT / 2.f + 50.f});

    Button backToMenuButton;
    setupButton(backToMenuButton, "Main Menu", {WINDOW_WIDTH / 2.f, WINDOW_HEIGHT / 2.f + 120.f});

    sf::Text pauseText;
    pauseText.setFont(font);
    pauseText.setString("PAUSED");
    pauseText.setCharacterSize(72);
    pauseText.setFillColor(sf::Color::White);
    sf::FloatRect b = pauseText.getLocalBounds();
    pauseText.setOrigin(b.left + b.width / 2.f, b.top + b.height / 2.f);
    float pauseAnimTime = 0.f;

    Button pauseContinueButton;
    Button pauseSaveSlotsButton;
    Button pauseMainMenuButton;

    const float PAUSE_BUTTON_SPACING = 70.f;
    float pauseY = WINDOW_HEIGHT / 2.f - PAUSE_BUTTON_SPACING * 0.5f;

    setupButton(pauseContinueButton, "Continue", {WINDOW_WIDTH / 2.f, pauseY});
    pauseY += PAUSE_BUTTON_SPACING;
    setupButton(pauseSaveSlotsButton, "Save", {WINDOW_WIDTH / 2.f, pauseY});
    pauseY += PAUSE_BUTTON_SPACING;
    setupButton(pauseMainMenuButton, "Main Menu", {WINDOW_WIDTH / 2.f, pauseY});
    pauseY += PAUSE_BUTTON_SPACING;

    sf::RectangleShape pauseOverlay({(float)WINDOW_WIDTH, (float)WINDOW_HEIGHT});
    pauseOverlay.setFillColor(sf::Color(0, 0, 0, 140));

    sf::Texture backgroundTexture;
    if (!backgroundTexture.loadFromFile("background.png"))
    {
        return -1;
    }
    sf::Sprite backgroundSprite(backgroundTexture);

    GameState currentState = MENU;
    GameState prevState = MENU;

    // Player init
    Player player;

    // Enemy init
    Enemy enemies[MAX_ENEMIES];
    int level = 1;
    int score = 0;
    int pauseChoice = 0;

    // Form direction
    float formationDir = 1.f;
    // Bullet
    BulletText pBullets[MAX_P_BULLETS];
    BulletText eBullets[MAX_E_BULLETS];

    auto spawnEnemy = [&](int lvl)
    {
        for (int i = 0; i < MAX_ENEMIES; ++i)
            enemies[i].active = false;

        formationDir = 1.f;
        if (currentMode == MODE_HARD)
        {
            if (lvl % 5 == 0)
            {
                Enemy e;
                e.active = true;
                e.boss = true;
                e.pos = {WINDOW_WIDTH / 2.f, 120.f};
                e.basePos = e.pos;
                e.t = 0.f;
                e.radius = 36.f;
                e.maxHp = 60 + 30 * lvl;
                e.hp = e.maxHp;
                if (lvl == 5)
                    e.bossType = 0;
                else if (lvl == 10)
                    e.bossType = 1;
                else if (lvl == 15)
                    e.bossType = 2;
                else if (lvl == 20)
                    e.bossType = 3;
                else if (lvl == 25)
                    e.bossType = 4;
                else // lvl >= 30
                    e.bossType = ((lvl / 5) - 1) % 5;
                e.fireCD = ENEMY_FIRE_BASE_COOLDOWN * 0.4f;
                e.attackMode = false;
                e.attackTimer = 0.f;
                enemies[0] = e;
            }
            else
            {
                int idx = 0;
                for (int r = 0; r < FORM_ROWS; r++)
                {
                    for (int c = 0; c < FORM_COLS; c++)
                    {
                        if (idx >= MAX_ENEMIES)
                            break;
                        Enemy e;
                        e.active = true;
                        e.basePos = e.pos;
                        e.gridX = c;
                        e.gridY = r;
                        e.boss = false;
                        e.t = static_cast<float>((r * 17 + c * 13) % 100) * 0.01f;
                        e.radius = 26.f;
                        e.pos = {FORM_START_X + c * FORM_GAP_X, FORM_START_Y + r * FORM_GAP_Y};
                        e.basePos = e.pos;
                        e.hp = std::max(10, 10 + 4 * lvl);
                        e.fireCD = ENEMY_FIRE_BASE_COOLDOWN + (std::rand() % 60) / 100.f;
                        e.hp *= 2;
                        e.fireCD *= (0.7f / (lvl / 2));
                        e.attackMode = false;
                        e.attackTimer = 0.f;
                        enemies[idx++] = e;
                    }
                }
            }
        }
        else if (currentMode == MODE_NORMAL)
        {
            if (lvl % 5 == 0)
            {
                Enemy e;
                e.active = true;
                e.boss = true;
                e.pos = {WINDOW_WIDTH / 2.f, 120.f};
                e.basePos = e.pos;
                e.t = 0.f;
                e.radius = 36.f;
                e.maxHp = 60 + 20 * lvl;
                e.hp = e.maxHp;
                if (lvl == 5)
                    e.bossType = 0;
                else if (lvl == 10)
                    e.bossType = 1;
                else if (lvl == 15)
                    e.bossType = 2;
                else if (lvl == 20)
                    e.bossType = 3;
                else if (lvl == 25)
                    e.bossType = 4;
                else // lvl >= 30
                    e.bossType = ((lvl / 5) - 1) % 5;
                e.fireCD = ENEMY_FIRE_BASE_COOLDOWN * 0.8f;
                e.attackMode = false;
                e.attackTimer = 0.f;
                enemies[0] = e;
            }
            else
            {
                int idx = 0;
                for (int r = 0; r < FORM_ROWS; r++)
                {
                    for (int c = 0; c < FORM_COLS; c++)
                    {
                        if (idx >= MAX_ENEMIES)
                            break;
                        Enemy e;
                        e.active = true;
                        e.basePos = e.pos;
                        e.gridX = c;
                        e.gridY = r;
                        e.boss = false;
                        e.t = static_cast<float>((r * 17 + c * 13) % 100) * 0.01f;
                        e.radius = 26.f;
                        e.pos = {FORM_START_X + c * FORM_GAP_X, FORM_START_Y + r * FORM_GAP_Y};
                        e.basePos = e.pos;
                        e.hp = std::max(10, 10 + 4 * lvl);
                        e.fireCD = ENEMY_FIRE_BASE_COOLDOWN + (std::rand() % 60) / 100.f;
                        e.attackMode = false;
                        e.attackTimer = 0.f;
                        enemies[idx++] = e;
                    }
                }
            }
        }
    };

    spawnEnemy(level);
    // Shooting
    auto makeBulletText = [&](BulletText &b, const sf::Vector2f &pos, int dmg, const sf::Color &col)
    {
        b.active = true;
        b.pos = pos;
        b.damage = dmg;
        b.text.setFont(font);
        std::string bin = toBinary(dmg);
        std::string vertical;
        vertical.reserve(bin.size() * 2);
        for (size_t i = 0; i < bin.size(); i++)
        {
            vertical.push_back(bin[i]);
            if (i != bin.size() - 1)
                vertical.push_back('\n');
        }
        b.text.setString(vertical);
        b.text.setCharacterSize(22);
        b.text.setFillColor(col);
        // Center
        auto bounds = b.text.getLocalBounds();
        b.text.setOrigin(bounds.left + bounds.width / 2.f, bounds.top + bounds.height / 2.f);
        b.text.setPosition(b.pos);
    };

    auto fireEnemyBullet = [&](Enemy &e)
    {
        // Damage scale by level
        int dmg = 1 + 2 * level;

        if (dmg > 64)
            dmg = 64;

        auto spawnBullet = [&](sf::Vector2f vel)
        {
            for (int i = 0; i < MAX_E_BULLETS; i++)
            {
                if (!eBullets[i].active)
                {
                    makeBulletText(eBullets[i], e.pos + sf::Vector2f(0.f, e.radius + 10.f), dmg, sf::Color::Red);
                    eBullets[i].vel = vel;
                    break;
                }
            }
        };

        for (int i = 0; i < MAX_E_BULLETS; ++i)
        {
            if (!eBullets[i].active)
            {
                spawnBullet({0.f, E_BULLET_SPEED});
                break;
            }
        }
    };
    auto fireBossBullet = [&](Enemy &e)
    {
        int dmg = 4 + 2 * level;
        if (dmg > 64)
            dmg = 64;

        // helper spawn bullet
        auto spawnBullet = [&](sf::Vector2f vel)
        {
            int idx = -1;
            for (int i = 0; i < MAX_E_BULLETS; i++)
            {
                if (!eBullets[i].active)
                {
                    idx = i;
                    break;
                }
            }
            if (idx == -1)
            {
                idx = 0;
            }

            makeBulletText(eBullets[idx], e.pos + sf::Vector2f(0.f, e.radius + 10.f), dmg, sf::Color::Red);
            eBullets[idx].vel = vel;
        };

        switch (e.bossType)
        {
        case 0: // Shooter Boss
            std::cout << "Executing fire pattern for BossType 0" << std::endl;
            if (e.phase == 1)
            {
                spawnBullet({0.f, E_BULLET_SPEED});
            }
            else if (e.phase == 2)
            {

                spawnBullet({0.f, E_BULLET_SPEED});
                spawnBullet({-60.f, E_BULLET_SPEED});
                spawnBullet({60.f, E_BULLET_SPEED});
            }
            else if (e.phase == 3)
            {

                for (int dx = -2; dx <= 2; dx++)
                    spawnBullet({dx * 40.f, E_BULLET_SPEED});
            }
            else if (e.phase == 4)
            {
                // spam 8 viên theo vòng
                static float angleOffset = 0.f;
                for (int i = 0; i < 8; i++)
                {
                    float angle = angleOffset + i * 3.14159f / 4.f;
                    spawnBullet({std::cos(angle) * E_BULLET_SPEED, std::sin(angle) * E_BULLET_SPEED});
                }
                angleOffset += 0.2f;
            }
            break;
        case 1: // Spread Boss

            std::cout << "Executing fire pattern for BossType 1" << std::endl;

            if (e.phase == 1)
            {

                spawnBullet({0.f, E_BULLET_SPEED});
                spawnBullet({-100.f, E_BULLET_SPEED});
                spawnBullet({100.f, E_BULLET_SPEED});
            }
            else if (e.phase == 2)
            {

                for (int dx = -2; dx <= 2; dx++)
                    spawnBullet({dx * 80.f, E_BULLET_SPEED});
            }
            else if (e.phase == 3)
            {
                // bắn 2 vòng đạn chéo
                for (int i = -3; i <= 3; i++)
                {
                    spawnBullet({i * 60.f, E_BULLET_SPEED});
                }
            }
            else if (e.phase == 4)
            {
                // bắn theo hình quạt rộng
                for (int i = -5; i <= 5; i++)
                {
                    spawnBullet({i * 50.f, E_BULLET_SPEED});
                }
            }
            break;
        case 2: // Summoner Boss
            std::cout << "Executing fire pattern for BossType 2" << std::endl;
            if (e.phase == 1)
            {
                spawnBullet({0.f, E_BULLET_SPEED});
            }
            else if (e.phase == 2)
            {
                spawnBullet({-80.f, E_BULLET_SPEED});
                spawnBullet({80.f, E_BULLET_SPEED});
            }
            else if (e.phase == 3)
            {
                // bắn + gọi thêm enemy nhỏ
                spawnBullet({0.f, E_BULLET_SPEED});
                for (int i = 0; i < MAX_ENEMIES; i++)
                {
                    if (!enemies[i].active)
                    {
                        Enemy minion;
                        minion.active = true;
                        minion.boss = false;
                        minion.pos = e.pos + sf::Vector2f((std::rand() % 100) - 50, 40.f);
                        minion.basePos = minion.pos;
                        minion.hp = 6 + level;
                        minion.radius = 18.f;
                        minion.fireCD = ENEMY_FIRE_BASE_COOLDOWN + (std::rand() % 100) / 100.f;
                        enemies[i] = minion;
                        break;
                    }
                }
            }
            else if (e.phase == 4)
            {
                // vừa summon vừa spam spread
                for (int dx = -2; dx <= 2; dx++)
                    spawnBullet({dx * 70.f, E_BULLET_SPEED});
                for (int i = 0; i < 2; i++)
                {
                    int idx = std::rand() % MAX_ENEMIES;
                    if (!enemies[idx].active)
                    {
                        Enemy minion;
                        minion.active = true;
                        minion.boss = false;
                        minion.pos = e.pos + sf::Vector2f((std::rand() % 200) - 100, 50.f);
                        minion.basePos = minion.pos;
                        minion.hp = 8 + level;
                        minion.radius = 20.f;
                        minion.fireCD = ENEMY_FIRE_BASE_COOLDOWN + (std::rand() % 100) / 100.f;
                        enemies[idx] = minion;
                    }
                }
            }
            break;
        case 3: // Charger Boss
            std::cout << "Executing fire pattern for BossType 3" << std::endl;
            if (e.phase == 1)
                spawnBullet({0.f, E_BULLET_SPEED});
            else if (e.phase == 2)
            {
                spawnBullet({-100.f, E_BULLET_SPEED});
                spawnBullet({100.f, E_BULLET_SPEED});
            }
            else if (e.phase == 3)
            {
                // bắn + chuẩn bị lao xuống
                spawnBullet({0.f, E_BULLET_SPEED});
                if (!e.attackMode)
                {
                    e.attackMode = true;
                    e.attackTimer = 0.f;
                }
            }
            else if (e.phase == 4)
            {
                // spam spread nhanh
                for (int dx = -3; dx <= 3; dx++)
                    spawnBullet({dx * 70.f, E_BULLET_SPEED});
                if (!e.attackMode)
                {
                    e.attackMode = true;
                    e.attackTimer = 0.f;
                }
            }
            break;

        case 4: // Laser Boss

            std::cout << "Executing fire pattern for BossType 4" << std::endl;
            if (e.phase == 1)
            {
                spawnBullet({0.f, E_BULLET_SPEED});
            }
            else if (e.phase == 2)
            {
                // 2 laser

                spawnBullet({0.f, E_BULLET_SPEED * 1.5f});
                spawnBullet({0.f, E_BULLET_SPEED * 1.2f});
            }
            else if (e.phase == 3)
            {
                // Laser

                for (int i = -2; i <= 2; i++)
                    spawnBullet({i * 20.f, E_BULLET_SPEED * 1.5f});
            }
            else if (e.phase == 4)
            {
                // Laser

                for (int i = -6; i <= 6; i++)
                    spawnBullet({i * 40.f, E_BULLET_SPEED * 1.6f});
            }
            break;
        }
    };

    // Input State
    bool canShoot = true;
    float attackSpawnCooldown = 0.f;
    float titleAnimTime = 0.f;

    // Items
    Item items[MAX_ITEMS];

    Explosion explosions[MAX_EXPLOSIONS];

    PickupEffect pickupEffects[MAX_PICKUP_EFFECTS];

    auto dropItemAt = [&](const sf::Vector2f &pos)
    {
        if ((std::rand() % 100) < DROP_CHANCE_PERCENT)
        {
            for (int i = 0; i < MAX_ITEMS; ++i)
            {
                if (!items[i].active)
                {
                    items[i].active = true;
                    items[i].pos = pos;
                    items[i].text.setFont(font);
                    items[i].text.setCharacterSize(20);
                    auto b = items[i].text.getLocalBounds();
                    items[i].text.setOrigin(b.left + b.width / 2.f, b.top + b.height / 2.f);
                    items[i].text.setPosition(items[i].pos);
                    int r = std::rand() % 20; // 4 loại item
                    if (r < 7)
                    {
                        items[i].text.setString("DMG+1");
                        items[i].text.setFillColor(sf::Color::Green);
                        items[i].type = ITEM_DMG;
                        break;
                    }
                    else if (r < 14 && r >= 7)
                    {
                        items[i].text.setString("Single");
                        items[i].text.setFillColor(sf::Color::Cyan);
                        items[i].type = ITEM_SINGLE;
                        break;
                    }
                    else if (r <= 16 && r >= 14)
                    {
                        items[i].text.setString("Double");
                        items[i].text.setFillColor(sf::Color::Magenta);
                        items[i].type = ITEM_DOUBLE;
                        break;
                    }
                    else if (r <= 18 && r > 16)
                    {
                        items[i].text.setString("Spread");
                        items[i].text.setFillColor(sf::Color::Yellow);
                        items[i].type = ITEM_SPREAD;
                        break;
                    }
                    else if (r <= 20 && r > 18)
                    {
                        items[i].text.setString("Heal");
                        items[i].text.setFillColor(sf::Color::Green);
                        items[i].type = HEAL;
                    }
                    break;
                }
            }
        }
    };

    auto triggerExplosion = [&](const sf::Vector2f &pos)
    {
        for (int i = 0; i < MAX_EXPLOSIONS; ++i)
        {
            if (!explosions[i].active)
            {
                explosions[i].active = true;
                explosionSound.play();
                explosions[i].pos = pos;
                explosions[i].currentFrame = 0;
                explosions[i].frameTimer = 0.f;
                break;
            }
        }
    };

    auto triggerPickupEffect = [&](const sf::Vector2f &pos, const sf::Color &color)
    {
        for (int i = 0; i < MAX_PICKUP_EFFECTS; ++i)
        {
            if (!pickupEffects[i].active)
            {
                powerupSound.play();
                pickupEffects[i].active = true;
                pickupEffects[i].pos = pos;
                pickupEffects[i].radius = 10.f;
                pickupEffects[i].alpha = 255.f;
                pickupEffects[i].color = color;
                break;
            }
        }
    };

    sf::Clock clock;

    while (window.isOpen())
    {
        // Handle Event
        float dt = clock.restart().asSeconds();
        sf::Event event;
        while (window.pollEvent(event))
        {
            if (currentState == HELP)
            {
                if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape)
                {
                    currentState = MENU;
                }
            }
            if (event.type == sf::Event::Closed)
            {
                window.close();
            }
            if (currentState == MENU && event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left)
            {
                sf::Vector2f mousePos = window.mapPixelToCoords({event.mouseButton.x, event.mouseButton.y});
                if (startButton.isHovered(mousePos))
                {
                    currentState = PLAYING;
                }
                if (helpButton.isHovered(mousePos))
                {
                    currentState = HELP;
                }
                if (saveSlotsButton.isHovered(mousePos))
                {
                    prevState = MENU;
                    currentState = SAVE_SLOTS;
                }
                if (highScoresButton.isHovered(mousePos))
                {
                    currentState = HIGH_SCORES;
                }
                if (modeButton.isHovered(mousePos))
                {
                    currentState = MODE;
                }
            }
        }

        if (currentState == MENU)
        {
            gameMusic.stop();
            if (menuMusic.getStatus() != sf::Music::Playing)
                menuMusic.play();
            for (auto &e : decoEnemies)
            {
                e.pos += e.vel * dt;
                if (e.pos.x < -e.radius)
                {
                    e.pos.x = WINDOW_WIDTH + e.radius;
                    e.pos.y = 50 + std::rand() % 300;
                }
                else if (e.pos.x > WINDOW_WIDTH + e.radius)
                {
                    e.pos.x = -e.radius;
                    e.pos.y = 50 + std::rand() % 300;
                }
            }

            window.clear();
            window.draw(backgroundSprite);
            sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

            updateButtonAppearance(startButton, mousePos);
            updateButtonAppearance(modeButton, mousePos);
            updateButtonAppearance(helpButton, mousePos);
            updateButtonAppearance(saveSlotsButton, mousePos);
            updateButtonAppearance(highScoresButton, mousePos);

            // Update button animations
            for (auto &button : {&startButton, &modeButton, &helpButton, &saveSlotsButton, &highScoresButton})
            {
                button->t += dt;
                sf::FloatRect textBounds = button->text.getLocalBounds(); // Khai báo `textBounds` ở đây

                // Hover effect: bobbing animation
                if (button->isHovered(mousePos))
                {
                    float bobbing = std::sin(button->t * 5.0f) * 5.0f;
                    button->rect.setOrigin(100.f, 25.f + bobbing);
                    button->text.setOrigin(textBounds.left + textBounds.width / 2.0f, textBounds.top + textBounds.height / 2.0f + bobbing);
                }
                else
                {
                    button->rect.setOrigin(100.f, 25.f);
                    button->text.setOrigin(textBounds.left + textBounds.width / 2.0f, textBounds.top + textBounds.height / 2.0f);
                }
            }
            sf::Font titleFont;
            if (!titleFont.loadFromFile("Pixel Game.otf"))
            {
                titleFont = font;
            }

            sf::Text gameTitle;
            gameTitle.setFont(titleFont);
            gameTitle.setString("NUMERIC INVASION");
            gameTitle.setCharacterSize(100); // chữ to
            gameTitle.setFillColor(sf::Color::White);

            // căn giữa
            sf::FloatRect tb = gameTitle.getLocalBounds();
            titleAnimTime += dt;
            gameTitle.setOrigin(tb.left + tb.width / 2.f, tb.top + tb.height / 2.f);
            float offsetY = std::sin(titleAnimTime * 2.f) * 10.f; // biên độ 10px
            gameTitle.setPosition(WINDOW_WIDTH / 2.f, WINDOW_HEIGHT / 2.f - 280.f + offsetY);

            sf::Uint8 r = 200 + std::sin(titleAnimTime * 2.f) * 55;
            sf::Uint8 g = 200 + std::sin(titleAnimTime * 3.f) * 55;
            sf::Uint8 b = 50 + std::sin(titleAnimTime * 4.f) * 50;

            gameTitle.setFillColor(sf::Color(r, g, b));

            window.draw(startButton.rect);
            window.draw(startButton.text);
            window.draw(modeButton.rect);
            window.draw(modeButton.text);
            window.draw(helpButton.rect);
            window.draw(helpButton.text);
            window.draw(saveSlotsButton.rect);
            window.draw(saveSlotsButton.text);
            window.draw(highScoresButton.rect);
            window.draw(highScoresButton.text);
            window.draw(gameTitle);
            for (auto &e : decoEnemies)
            {
                sf::Sprite dome(texEnemy);
                dome.setOrigin(dome.getTexture()->getSize().x / 2.f, dome.getTexture()->getSize().y / 2.f);
                dome.setPosition(e.pos.x, e.pos.y - e.radius * 0.2f);
                window.draw(dome);
            }
        }
        else if (currentState == PLAYING)
        {
            if (event.type == sf::Event::Closed)
            {
                window.close();
            }
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape)
            {
                currentState = PAUSED;
            }
            menuMusic.stop();
            if (gameMusic.getStatus() != sf::Music::Playing)
                gameMusic.play();

            attackSpawnCooldown -= dt;

            float dir = 0.f;
            if (attackSpawnCooldown < 0.f)
                attackSpawnCooldown = 0.f;
            // Player movement
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left) || sf::Keyboard::isKeyPressed(sf::Keyboard::A))
                dir -= 1.f;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right) || sf::Keyboard::isKeyPressed(sf::Keyboard::D))
                dir += 1.f;

            player.pos.x += dir * PLAYER_SPEED * dt;

            if (player.pos.x < player.radius)
                player.pos.x = player.radius;
            if (player.pos.x > WINDOW_WIDTH - player.radius)
                player.pos.x = WINDOW_WIDTH - player.radius;

            // Enemy movement

            // Shoot bullet (Space)
            if (style == SINGLE)
            {
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Space))
                {
                    if (canShoot)
                    {
                        for (int i = 0; i < MAX_P_BULLETS; i++)
                        {
                            if (!pBullets[i].active)
                            {
                                shootSound.play();
                                makeBulletText(pBullets[i], player.pos - sf::Vector2f(0.f, player.radius + 8.f), player.damage, sf::Color::Yellow);
                                pBullets[i].vel = {0.f, -P_BULLET_SPEED}; // thẳng lên
                                break;
                            }
                        }
                        canShoot = false;
                    }
                }
                else
                {
                    canShoot = true;
                }
            }
            else if (style == DOUBLE)
            {
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Space))
                {
                    if (canShoot)
                    {
                        // 2 rows of bullets
                        for (int i = 0; i < MAX_P_BULLETS; i++)
                        {
                            if (!pBullets[i].active)
                            {
                                shootSound.play();
                                makeBulletText(pBullets[i], player.pos + sf::Vector2f(-15.f, -player.radius - 8.f),
                                               player.damage, sf::Color::Yellow);
                                pBullets[i].vel = {0.f, -P_BULLET_SPEED}; // thẳng lên
                                break;
                            }
                        }
                        for (int i = 0; i < MAX_P_BULLETS; i++)
                        {
                            if (!pBullets[i].active)
                            {
                                makeBulletText(pBullets[i], player.pos + sf::Vector2f(15.f, -player.radius - 8.f),
                                               player.damage, sf::Color::Yellow);
                                pBullets[i].vel = {0.f, -P_BULLET_SPEED}; // thẳng lên
                                break;
                            }
                        }
                        canShoot = false;
                    }
                }
                else
                {
                    canShoot = true;
                }
            }
            else if (style == SPREAD)
            {
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Space))
                {
                    if (canShoot)
                    {
                        sf::Vector2f basePos = player.pos - sf::Vector2f(0.f, player.radius + 8.f);

                        // Straight
                        for (int i = 0; i < MAX_P_BULLETS; i++)
                        {
                            if (!pBullets[i].active)
                            {
                                shootSound.play();
                                makeBulletText(pBullets[i], basePos, player.damage, sf::Color::Yellow);
                                pBullets[i].vel = {0.f, -P_BULLET_SPEED};
                                break;
                            }
                        }

                        // Left
                        for (int i = 0; i < MAX_P_BULLETS; i++)
                        {
                            if (!pBullets[i].active)
                            {
                                makeBulletText(pBullets[i], basePos, player.damage, sf::Color::Yellow);
                                pBullets[i].vel = {-120.f, -P_BULLET_SPEED}; // lệch trái
                                break;
                            }
                        }

                        // right
                        for (int i = 0; i < MAX_P_BULLETS; i++)
                        {
                            if (!pBullets[i].active)
                            {
                                makeBulletText(pBullets[i], basePos, player.damage, sf::Color::Yellow);
                                pBullets[i].vel = {120.f, -P_BULLET_SPEED}; // lệch phải
                                break;
                            }
                        }
                        canShoot = false;
                    }
                }
                else
                {
                    canShoot = true;
                }
            }
            // Move player bullets
            for (int i = 0; i < MAX_P_BULLETS; ++i)
            {
                if (!pBullets[i].active)
                    continue;
                pBullets[i].pos += pBullets[i].vel * dt;
                pBullets[i].text.setPosition(pBullets[i].pos);
                if (pBullets[i].pos.y < -40.f)
                {
                    pBullets[i].active = false;
                    continue;
                }
                // hit enemy
                for (int ei = 0; ei < MAX_ENEMIES; ++ei)
                {
                    Enemy &e = enemies[ei];
                    if (!e.active)
                        continue;
                    if (circleHit(pBullets[i].pos, pBullets[i].radius, e.pos, e.radius))
                    {
                        e.hp -= pBullets[i].damage;
                        e.hitTimer = 0.1f;
                        pBullets[i].active = false;
                        if (e.hp <= 0)
                        {
                            dropItemAt(e.pos);
                            triggerExplosion(e.pos);
                            e.active = false;
                            score += (e.boss ? 150 : 10);
                        }
                        break;
                    }
                }
            }
            if (currentMode == MODE_SURVIVAL)
            {
                // Formation movement
                float minX = 1e9f, maxX = -1e9f;
                bool anyEnemyAlive = false;
                for (int i = 0; i < MAX_ENEMIES; ++i)
                {
                    if (!enemies[i].active)
                        continue;
                    anyEnemyAlive = true;
                    minX = std::min(minX, enemies[i].pos.x - enemies[i].radius);
                    maxX = std::max(maxX, enemies[i].pos.x + enemies[i].radius);
                }
                // Zig Zag move down
                if (anyEnemyAlive)
                {
                    if (minX < FORM_MARGIN && formationDir < 0)
                    {
                        formationDir = 1;
                        for (int i = 0; i < MAX_ENEMIES; i++)
                        {
                            if (enemies[i].active)
                            {
                                enemies[i].pos.y += 0;
                            }
                        }
                    }
                    else if (maxX > WINDOW_WIDTH - FORM_MARGIN && formationDir > 0)
                    {
                        formationDir = -1;
                        for (int i = 0; i < MAX_ENEMIES; i++)
                        {
                            if (enemies[i].active && !enemies[i].attackMode)
                                enemies[i].pos.y += 0;
                        }
                    }
                }
                // Random Enemy attack
                if (attackSpawnCooldown <= 0.f)
                {
                    if ((std::rand() % 300) == 0)
                    {
                        int start = std::rand() % MAX_ENEMIES;
                        for (int k = 0; k < MAX_ENEMIES; ++k)
                        {
                            int i = (start + k) % MAX_ENEMIES;
                            Enemy &cand = enemies[i];
                            if (cand.active && !cand.boss && !cand.attackMode)
                            {
                                cand.attackMode = true;
                                cand.attackTimer = 0.f;
                                attackSpawnCooldown = 1.2f;
                                break;
                            }
                        }
                    }
                }
                // Update enemies
                for (int i = 0; i < MAX_ENEMIES; ++i)
                {
                    Enemy &e = enemies[i];
                    if (!e.active)
                        continue;
                    if (!e.boss)
                    {
                        if (e.hitTimer > 0.f)
                        {
                            e.hitTimer -= dt;
                            if (e.hitTimer < 0.f)
                                e.hitTimer = 0.f;
                        }
                    }
                    if (e.boss)
                    {
                        float hpPercent = (float)e.hp / (float)e.maxHp;
                        if (hpPercent > 0.75f)
                            e.phase = 1;
                        else if (hpPercent > 0.5f)
                            e.phase = 2;
                        else if (hpPercent > 0.25f)
                            e.phase = 3;
                        else
                            e.phase = 4;
                    }
                    if (e.attackMode && !e.boss)
                    {
                        const float DIVE_SPEED = 300.f;
                        e.pos.y += DIVE_SPEED * dt;

                        if (circleHit(e.pos, e.radius, player.pos, player.radius))
                        {
                            player.hp -= 10;
                            crashSound.play();
                            if (player.hp < 0)
                            {
                                player.hp = 0;
                                deathSound.play();
                            }

                            e.active = false;
                            continue;
                        }

                        if (e.pos.y > WINDOW_HEIGHT - 30.f)
                        {
                            sf::Vector2f shift(0.f, 0.f);
                            bool found = false;
                            for (int j = 0; j < MAX_ENEMIES; ++j)
                            {
                                const Enemy &o = enemies[j];
                                if (!o.active || o.boss || o.attackMode || o.returning)
                                    continue;
                                shift = (o.pos - o.basePos);
                                found = true;
                                break;
                            }
                            if (!found)
                                shift = sf::Vector2f(0.f, 0.f);
                            e.attackMode = false;
                            e.returning = true;
                            e.attackTimer = 0.f;
                        }
                    }
                    if (e.returning)
                    {
                        sf::Vector2f shift(0.f, 0.f);
                        bool found = false;
                        for (int j = 0; j < MAX_ENEMIES; ++j)
                        {
                            const Enemy &o = enemies[j];
                            if (!o.active || o.boss || o.attackMode || o.returning)
                                continue;
                            shift = (o.pos - o.basePos);
                            found = true;
                            break;
                        }
                        if (!found)
                            shift = sf::Vector2f(0.f, 0.f);

                        sf::Vector2f target = e.basePos + shift;

                        sf::Vector2f dir = target - e.pos;
                        float dist = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                        if (dist < 5.f)
                        {
                            e.pos = target;
                            e.returning = false;
                        }
                        else
                        {
                            dir /= dist;
                            float speed = 200.f;
                            e.pos += dir * speed * dt;
                        }
                    }
                    else
                    {
                        e.pos.x += formationDir * FORM_SPEED * dt;
                    }
                    e.t += dt;
                    e.fireCD -= dt;
                    if (e.fireCD <= 0.f)
                    {
                        if (e.boss)
                        {
                            fireBossBullet(e);
                            if (e.phase == 1)
                                e.fireCD = ENEMY_FIRE_BASE_COOLDOWN * 0.8f;
                            else if (e.phase == 2)
                                e.fireCD = ENEMY_FIRE_BASE_COOLDOWN * 0.7f;
                            else if (e.phase == 3)
                                e.fireCD = ENEMY_FIRE_BASE_COOLDOWN * 0.6f;
                            else
                                e.fireCD = ENEMY_FIRE_BASE_COOLDOWN * 0.5f;
                        }
                        else
                        {
                            fireEnemyBullet(e);
                            e.fireCD = ENEMY_FIRE_BASE_COOLDOWN + (std::rand() % 60) / 100.f;
                        }
                    }
                }
                // dùng dt đã tính ở đầu vòng lặp (không restart lại clock)
                survivalTimer += dt;
                enemySpawnCD -= dt;

                // Spawn 1 enemy đơn lẻ (không xóa cả mảng)
                if (enemySpawnCD <= 0.f)
                {
                    for (int si = 0; si < MAX_ENEMIES; ++si)
                    {
                        if (!enemies[si].active)
                        {
                            Enemy en;
                            en.active = true;
                            en.boss = false;
                            en.gridX = -1;
                            en.gridY = -1;
                            en.t = static_cast<float>((si * 17 + (std::rand() % 100)) % 100) * 0.01f;
                            en.radius = 26.f;
                            float x = 50.f + (std::rand() % (WINDOW_WIDTH - 100));
                            en.pos = {x, 80.f}; // spawn từ trên màn hình
                            en.basePos = en.pos;
                            en.hp = std::max(6, 6 + (int)(survivalTimer / 20.0f)); // tăng hp dần
                            en.fireCD = ENEMY_FIRE_BASE_COOLDOWN + (std::rand() % 60) / 100.f;
                            en.attackMode = false;
                            en.attackTimer = 0.f;
                            en.maxHp = en.hp;
                            enemies[si] = en;
                            break;
                        }
                    }
                    enemySpawnCD = std::max(0.5f, 2.f - survivalTimer / 30.f); // spawn nhanh dần
                }

                // Spawn boss mỗi 60s (tăng bossCycle, chỉ spawn 1 boss khi bước sang mốc kế)
                int bossCycle = (int)survivalTimer / 60;
                if (bossCycle > lastBossSpawn)
                {
                    // tìm slot trống (hoặc overwrite slot 0 nếu full)
                    int idx = -1;
                    for (int i = 0; i < MAX_ENEMIES; ++i)
                        if (!enemies[i].active)
                        {
                            idx = i;
                            break;
                        }
                    if (idx == -1)
                        idx = 0;

                    Enemy boss;
                    boss.active = true;
                    boss.boss = true;
                    boss.pos = {WINDOW_WIDTH / 2.f, 120.f};
                    boss.basePos = boss.pos;
                    boss.t = 0.f;
                    boss.radius = 36.f;
                    boss.maxHp = 80 + 30 * bossCycle; // tăng theo lần boss
                    boss.hp = boss.maxHp;
                    boss.bossType = bossCycle % 5; // tuần tự các type
                    boss.fireCD = ENEMY_FIRE_BASE_COOLDOWN * 0.6f;
                    boss.attackMode = false;
                    boss.attackTimer = 0.f;
                    enemies[idx] = boss;

                    lastBossSpawn = bossCycle;
                }

                // Score tính bằng thời gian sống (10 điểm / giây)
                score = (int)survivalTimer * 10;
            }
            else
            {
                // Formation movement
                float minX = 1e9f, maxX = -1e9f;
                bool anyEnemyAlive = false;
                for (int i = 0; i < MAX_ENEMIES; ++i)
                {
                    if (!enemies[i].active)
                        continue;
                    anyEnemyAlive = true;
                    minX = std::min(minX, enemies[i].pos.x - enemies[i].radius);
                    maxX = std::max(maxX, enemies[i].pos.x + enemies[i].radius);
                }
                // Zig Zag move down
                if (anyEnemyAlive)
                {
                    if (minX < FORM_MARGIN && formationDir < 0)
                    {
                        formationDir = 1;
                        for (int i = 0; i < MAX_ENEMIES; i++)
                        {
                            if (enemies[i].active)
                            {
                                enemies[i].pos.y += FORM_DROP_Y;
                            }
                        }
                    }
                    else if (maxX > WINDOW_WIDTH - FORM_MARGIN && formationDir > 0)
                    {
                        formationDir = -1;
                        for (int i = 0; i < MAX_ENEMIES; i++)
                        {
                            if (enemies[i].active && !enemies[i].attackMode)
                                enemies[i].pos.y += FORM_DROP_Y;
                        }
                    }
                }
                // Random Enemy attack
                if (attackSpawnCooldown <= 0.f)
                {
                    if ((std::rand() % 300) == 0)
                    {
                        int start = std::rand() % MAX_ENEMIES;
                        for (int k = 0; k < MAX_ENEMIES; ++k)
                        {
                            int i = (start + k) % MAX_ENEMIES;
                            Enemy &cand = enemies[i];
                            if (cand.active && !cand.boss && !cand.attackMode)
                            {
                                cand.attackMode = true;
                                cand.attackTimer = 0.f;
                                attackSpawnCooldown = 1.2f;
                                break;
                            }
                        }
                    }
                }
                // Update enemies
                for (int i = 0; i < MAX_ENEMIES; ++i)
                {
                    Enemy &e = enemies[i];
                    if (!e.active)
                        continue;
                    if (e.boss)
                    {
                        float hpPercent = (float)e.hp / (float)e.maxHp;
                        if (hpPercent > 0.75f)
                            e.phase = 1;
                        else if (hpPercent > 0.5f)
                            e.phase = 2;
                        else if (hpPercent > 0.25f)
                            e.phase = 3;
                        else
                            e.phase = 4;
                    }
                    if (e.attackMode && !e.boss)
                    {
                        const float DIVE_SPEED = 300.f;
                        e.pos.y += DIVE_SPEED * dt;

                        if (circleHit(e.pos, e.radius, player.pos, player.radius))
                        {
                            player.hp -= 10;
                            crashSound.play();
                            if (player.hp < 0)
                            {
                                player.hp = 0;
                                deathSound.play();
                            }
                            e.active = false;
                            continue;
                        }

                        if (e.pos.y > WINDOW_HEIGHT - 30.f)
                        {
                            sf::Vector2f shift(0.f, 0.f);
                            bool found = false;
                            for (int j = 0; j < MAX_ENEMIES; ++j)
                            {
                                const Enemy &o = enemies[j];
                                if (!o.active || o.boss || o.attackMode || o.returning)
                                    continue;
                                shift = (o.pos - o.basePos);
                                found = true;
                                break;
                            }
                            if (!found)
                                shift = sf::Vector2f(0.f, 0.f);
                            e.attackMode = false;
                            e.returning = true;
                            e.attackTimer = 0.f;
                        }
                    }
                    if (e.returning)
                    {
                        sf::Vector2f shift(0.f, 0.f);
                        bool found = false;
                        for (int j = 0; j < MAX_ENEMIES; ++j)
                        {
                            const Enemy &o = enemies[j];
                            if (!o.active || o.boss || o.attackMode || o.returning)
                                continue;
                            shift = (o.pos - o.basePos);
                            found = true;
                            break;
                        }
                        if (!found)
                            shift = sf::Vector2f(0.f, 0.f);

                        sf::Vector2f target = e.basePos + shift;

                        sf::Vector2f dir = target - e.pos;
                        float dist = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                        if (dist < 5.f)
                        {
                            e.pos = target;
                            e.returning = false;
                        }
                        else
                        {
                            dir /= dist;
                            float speed = 200.f;
                            e.pos += dir * speed * dt;
                        }
                    }
                    else
                    {
                        e.pos.x += formationDir * FORM_SPEED * dt;
                    }
                    e.t += dt;
                    e.fireCD -= dt;
                    if (e.fireCD <= 0.f)
                    {
                        if (e.boss)
                        {
                            fireBossBullet(e);
                            if (e.phase == 1)
                                e.fireCD = ENEMY_FIRE_BASE_COOLDOWN * 0.8f;
                            else if (e.phase == 2)
                                e.fireCD = ENEMY_FIRE_BASE_COOLDOWN * 0.7f;
                            else if (e.phase == 3)
                                e.fireCD = ENEMY_FIRE_BASE_COOLDOWN * 0.6f;
                            else
                                e.fireCD = ENEMY_FIRE_BASE_COOLDOWN * 0.5f;
                        }
                        else
                        {
                            fireEnemyBullet(e);
                            e.fireCD = ENEMY_FIRE_BASE_COOLDOWN + (std::rand() % 60) / 100.f;
                        }
                    }
                }
                if (!anyEnemyAlive)
                {
                    level += 1;
                    spawnEnemy(level);
                }
            }
            // Enemies bullets
            for (int i = 0; i < MAX_E_BULLETS; ++i)
            {
                if (!eBullets[i].active)
                    continue;
                eBullets[i].pos += eBullets[i].vel * dt;
                eBullets[i].text.setPosition(eBullets[i].pos);
                if (eBullets[i].pos.y > WINDOW_HEIGHT + 40.f || eBullets[i].pos.x < -40.f ||
                    eBullets[i].pos.x > WINDOW_WIDTH + 40.f || eBullets[i].pos.y < -40.f)
                    eBullets[i].active = false;

                if (circleHit(eBullets[i].pos, eBullets[i].radius, player.pos, player.radius))
                {
                    player.hp -= eBullets[i].damage;
                    hitSound.play();
                    if (player.hp < 0)
                    {
                        player.hp = 0;
                        deathSound.play();
                    }
                    eBullets[i].active = false;
                }
            }

            // Items fall
            for (int i = 0; i < MAX_ITEMS; ++i)
            {
                if (!items[i].active)
                    continue;
                items[i].pos.y += ITEM_FALL_SPEED * dt;
                items[i].text.setPosition(items[i].pos);
                if (items[i].pos.y > WINDOW_HEIGHT + 30.f)
                {
                    items[i].active = false;
                    continue;
                }
                if (circleHit(items[i].pos, items[i].radius, player.pos, player.radius))
                {
                    triggerPickupEffect(player.pos, items[i].text.getFillColor());
                    if (items[i].type == ITEM_DMG)
                    {
                        player.damage += 1;
                    }
                    else if (items[i].type == ITEM_SINGLE)
                    {
                        if (style == ShootingStyle::SINGLE)
                        {
                            style = ShootingStyle::SINGLE;
                            player.damage += 1; // Cộng thêm 1 sát thương nếu đã là SINGLE
                        }
                        else
                        {
                            style = ShootingStyle::SINGLE;
                        }
                    }
                    else if (items[i].type == ITEM_DOUBLE)
                    {
                        if (style == ShootingStyle::DOUBLE)
                        {
                            style = ShootingStyle::DOUBLE;
                            player.damage += 1; // Cộng thêm 1 sát thương nếu đã là DOUBLE
                        }
                        else
                        {
                            style = ShootingStyle::DOUBLE;
                        }
                    }
                    else if (items[i].type == ITEM_SPREAD)
                    {
                        if (style == ShootingStyle::SPREAD)
                        {
                            style = ShootingStyle::SPREAD;
                            player.damage += 1; // Cộng thêm 1 sát thương nếu đã là SPREAD
                        }
                        else
                        {
                            style = ShootingStyle::SPREAD;
                        }
                    }
                    else if (items[i].type == HEAL)
                    {
                        player.hp += 20;
                        if (player.hp >= 100)
                        {
                            player.hp = 100;
                        }
                    }
                    items[i].active = false;
                }
            }
            // Death check
            if (player.hp <= 0)
            {
                currentState = GAME_OVER;
                addHighScore(score, level, currentMode);
            }

            // Rendering
            window.clear();
            window.draw(backgroundSprite);
            for (auto &s : stars)
            {
                s.pos.y += s.speed * dt;
                if (s.pos.y > WINDOW_HEIGHT)
                {
                    s.pos.y = -s.radius;
                    s.pos.y = static_cast<float>(std::rand() % WINDOW_WIDTH);
                }
            }
            for (auto &s : stars)
            {
                sf::CircleShape star(s.radius);
                star.setPosition(s.pos);
                star.setFillColor(s.color);
                window.draw(star);
            }
            // Draw Explosion
            for (int i = 0; i < MAX_EXPLOSIONS; ++i)
            {
                if (!explosions[i].active)
                    continue;

                explosions[i].frameTimer += dt;
                if (explosions[i].frameTimer >= EXPLOSION_FRAME_DURATION)
                {
                    explosions[i].frameTimer = 0.f;
                    explosions[i].currentFrame++;
                    if (explosions[i].currentFrame >= EXPLOSION_FRAMES)
                    {
                        explosions[i].active = false; // Animation kết thúc
                    }
                }
            }
            for (int i = 0; i < MAX_EXPLOSIONS; ++i)
            {
                if (explosions[i].active)
                {
                    sf::Sprite explosionSprite(texExplosion);

                    sf::Vector2u textureSize = texExplosion.getSize();

                    int frameWidth = textureSize.x / EXPLOSION_FRAMES_PER_ROW;
                    int frameHeight = textureSize.y / EXPLOSION_FRAMES_PER_ROW;

                    int frame = explosions[i].currentFrame;
                    int row = frame / EXPLOSION_FRAMES_PER_ROW;
                    int col = frame % EXPLOSION_FRAMES_PER_ROW;

                    explosionSprite.setTextureRect(sf::IntRect(col * frameWidth, row * frameHeight, frameWidth, frameHeight));

                    explosionSprite.setOrigin(frameWidth / 2.f, frameHeight / 2.f);
                    explosionSprite.setPosition(explosions[i].pos);
                    explosionSprite.setScale(0.7f, 0.7f);

                    window.draw(explosionSprite);
                }
            }

            // Draw pickup effect
            for (int i = 0; i < MAX_PICKUP_EFFECTS; ++i)
            {
                if (!pickupEffects[i].active)
                    continue;

                pickupEffects[i].radius += EFFECT_EXPAND_SPEED * dt;
                pickupEffects[i].alpha -= EFFECT_FADE_SPEED * dt;

                if (pickupEffects[i].alpha <= 0)
                {
                    pickupEffects[i].active = false;
                }
            }

            sf::CircleShape effectCircle;
            for (int i = 0; i < MAX_PICKUP_EFFECTS; ++i)
            {
                if (pickupEffects[i].active)
                {
                    effectCircle.setRadius(pickupEffects[i].radius);
                    effectCircle.setOrigin(pickupEffects[i].radius, pickupEffects[i].radius);
                    effectCircle.setPosition(pickupEffects[i].pos);

                    sf::Color color = pickupEffects[i].color;
                    color.a = static_cast<sf::Uint8>(pickupEffects[i].alpha);

                    effectCircle.setFillColor(sf::Color::Transparent);
                    effectCircle.setOutlineColor(color);
                    effectCircle.setOutlineThickness(3.f); // Độ dày của viền

                    window.draw(effectCircle);
                }
            }

            // Draw player
            {
                sf::Sprite ship(texPlayer);
                ship.setOrigin(texPlayer.getSize().x / 2.f, texPlayer.getSize().y / 2.f);
                ship.setPosition(player.pos);
                ship.setScale(1.3f, 1.3f);
                window.draw(ship);
            }
            // Draw UFO
            for (int i = 0; i < MAX_ENEMIES; ++i)
            {
                Enemy &e = enemies[i];
                if (!e.active)
                    continue;

                float bob = std::sin(e.t * 2.1f + i) * (e.boss ? 10.f : 6.f);
                sf::Sprite dome(e.boss ? texBoss : texEnemy);
                dome.setOrigin(dome.getTexture()->getSize().x / 2.f, dome.getTexture()->getSize().y / 2.f);
                dome.setPosition(e.pos.x, e.pos.y + bob - e.radius * 0.2f);
                if (e.boss)
                {
                    dome.setScale(2.0f, 2.0f);
                    float bossHPPercent = static_cast<float>(e.hp) / static_cast<float>(e.maxHp);
                    sf::RectangleShape healthBarBossBackground({1000, 40});
                    healthBarBossBackground.setFillColor(sf::Color(100, 100, 100));
                    healthBarBossBackground.setPosition(WINDOW_WIDTH / 2.f - 500.f, 35.f);

                    sf::RectangleShape healthBarBoss({1000 * bossHPPercent, 40});
                    healthBarBoss.setFillColor(sf::Color::Red);
                    healthBarBoss.setPosition(WINDOW_WIDTH / 2.f - 500.f, 35.f);

                    sf::Text bossTitle("Boss HP", font, 40);
                    bossTitle.setFillColor(sf::Color::Yellow);
                    bossTitle.setPosition(WINDOW_WIDTH / 2.f - 40.f, 30.f);

                    window.draw(healthBarBossBackground);
                    window.draw(healthBarBoss);
                    window.draw(bossTitle);
                }
                else
                {
                    dome.setScale(1.3f, 1.3f);
                }

                window.draw(dome);
            }
            // Draw Bullets
            // Player Bullets
            for (int i = 0; i < MAX_P_BULLETS; ++i)
                if (pBullets[i].active)
                    window.draw(pBullets[i].text);
            // Enemy Bullets
            for (int i = 0; i < MAX_E_BULLETS; ++i)
                if (eBullets[i].active)
                    window.draw(eBullets[i].text);
            // Draw Items
            for (int i = 0; i < MAX_ITEMS; ++i)
                if (items[i].active)
                    window.draw(items[i].text);
            // UI
            sf::Text hud;
            hud.setFont(font);
            hud.setCharacterSize(20);

            float hpPercent = static_cast<float>(player.hp) / 100.0f;
            sf::RectangleShape healthBarBackground({200, 20});
            healthBarBackground.setFillColor(sf::Color(100, 100, 100));
            healthBarBackground.setPosition(10.f, WINDOW_HEIGHT - 40.f);

            sf::RectangleShape healthBar({200 * hpPercent, 20});
            healthBar.setFillColor(sf::Color::Green);
            healthBar.setPosition(10.f, WINDOW_HEIGHT - 40.f);

            window.draw(healthBarBackground);
            window.draw(healthBar);

            hud.setFillColor(sf::Color::Yellow);
            hud.setString("DMG: " + std::to_string(player.damage) + " (" + toBinary(player.damage) + ")");
            hud.setPosition(10.f, WINDOW_HEIGHT - 90.f);
            window.draw(hud);

            hud.setFillColor(sf::Color::Yellow);
            hud.setString("HP: " + std::to_string(player.hp) + "%");
            hud.setPosition(10.f, WINDOW_HEIGHT - 70.f);
            window.draw(hud);

            if (currentMode == MODE_SURVIVAL)
            {
                hud.setFillColor(sf::Color::White);
                hud.setString("   Time: " + std::to_string((int)survivalTimer) + "s");
                hud.setPosition(700.f, WINDOW_HEIGHT - 34.f);
                window.draw(hud);
            }
            else
            {
                hud.setFillColor(sf::Color::White);
                hud.setString("Level: " + std::to_string(level));
                hud.setPosition(700.f, WINDOW_HEIGHT - 40.f);
                window.draw(hud);
            }
            hud.setCharacterSize(40);
            hud.setFillColor(sf::Color::White);
            hud.setString("Score: " + std::to_string(score));
            hud.setPosition(10.f, 20.f);
            window.draw(hud);
            hud.setCharacterSize(20);

            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Escape))
            {
                currentState = GameState::PAUSED;
            }
        }
        else if (currentState == GAME_OVER)
        {
            if (event.type == sf::Event::Closed)
            {
                window.close();
            }
            if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left)
            {
                sf::Vector2f mousePos = window.mapPixelToCoords({event.mouseButton.x, event.mouseButton.y});
                if (restartButton.isHovered(mousePos))
                {
                    auto spawnFunc = [&](int lvl)
                    { spawnEnemy(lvl); };
                    resetGame(player, level, score, pBullets, eBullets, items, spawnFunc, style);
                    currentState = PLAYING;
                }
                if (backToMenuButton.isHovered(mousePos))
                {
                    auto spawnFunc = [&](int lvl)
                    { spawnEnemy(lvl); };
                    resetGame(player, level, score, pBullets, eBullets, items, spawnFunc, style);
                    currentState = MENU;
                }
            }
            gameMusic.stop();
            if (menuMusic.getStatus() != sf::Music::Playing)
                menuMusic.play();

            // Update UI
            sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
            updateButtonAppearance(restartButton, mousePos);
            updateButtonAppearance(backToMenuButton, mousePos);

            // Update final score text
            finalScoreText.setString("Your Score: " + std::to_string(score));
            sf::FloatRect scoreBounds = finalScoreText.getLocalBounds();
            finalScoreText.setOrigin(scoreBounds.left + scoreBounds.width / 2.f, scoreBounds.top + scoreBounds.height / 2.f);
            finalScoreText.setPosition(WINDOW_WIDTH / 2.f, WINDOW_HEIGHT / 2.f - 50.f);
            for (auto &e : decoEnemies)
            {
                e.pos += e.vel * dt;
                if (e.pos.x < -e.radius)
                {
                    e.pos.x = WINDOW_WIDTH + e.radius;
                    e.pos.y = 50 + std::rand() % 300;
                }
                else if (e.pos.x > WINDOW_WIDTH + e.radius)
                {
                    e.pos.x = -e.radius;
                    e.pos.y = 50 + std::rand() % 300;
                }
            }

            // Game Over screen rendering
            window.clear();
            window.draw(backgroundSprite);
            for (auto &e : decoEnemies)
            {
                sf::Sprite dome(texEnemy);
                dome.setOrigin(dome.getTexture()->getSize().x / 2.f, dome.getTexture()->getSize().y / 2.f);
                dome.setPosition(e.pos.x, e.pos.y - e.radius * 0.2f);
                window.draw(dome);
            }
            window.draw(gameOverText);
            window.draw(finalScoreText);
            window.draw(restartButton.rect);
            window.draw(restartButton.text);
            window.draw(backToMenuButton.rect);
            window.draw(backToMenuButton.text);
        }
        else if (currentState == GameState::PAUSED)
        {
            if (event.type == sf::Event::Closed)
            {
                window.close();
            }
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape)
            {
                currentState = PLAYING;
            }

            // clicked
            if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left)
            {
                sf::Vector2f mousePos = window.mapPixelToCoords({event.mouseButton.x, event.mouseButton.y});

                if (pauseContinueButton.isHovered(mousePos))
                {
                    currentState = PLAYING;
                }
                else if (pauseSaveSlotsButton.isHovered(mousePos))
                {
                    currentState = MENU;
                }
                else if (pauseMainMenuButton.isHovered(mousePos))
                {
                    auto spawnFunc = [&](int lvl)
                    { spawnEnemy(lvl); };
                    resetGame(player, level, score, pBullets, eBullets, items, spawnFunc, style);
                    currentState = MENU;
                }
                if (pauseSaveSlotsButton.isHovered(mousePos))
                {
                    prevState = PLAYING; // vì từ trong game nhảy ra
                    currentState = SAVE_SLOTS;
                }
            }
            // Hover effect
            {
                sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
                titleAnimTime += dt;
                float pauseOffsetY = std::sin(titleAnimTime * 2.f) * 10.f; // biên độ 10px
                pauseText.setPosition(WINDOW_WIDTH / 2.f, WINDOW_HEIGHT / 2.f - 160.f + pauseOffsetY);
                updateButtonAppearance(pauseContinueButton, mousePos);
                updateButtonAppearance(pauseSaveSlotsButton, mousePos);
                updateButtonAppearance(pauseMainMenuButton, mousePos);

                window.clear();
                window.draw(backgroundSprite);
                window.draw(pauseOverlay);
                window.draw(pauseText);
                window.draw(pauseContinueButton.rect);
                window.draw(pauseContinueButton.text);

                window.draw(pauseSaveSlotsButton.rect);
                window.draw(pauseSaveSlotsButton.text);

                window.draw(pauseMainMenuButton.rect);
                window.draw(pauseMainMenuButton.text);
            }
        }
        else if (currentState == HELP)
        {
            window.clear(sf::Color(20, 20, 40));

            sf::Text title("HELP", font, 60);
            title.setFillColor(sf::Color::Yellow);
            title.setPosition(WINDOW_WIDTH / 2.f - title.getGlobalBounds().width / 2.f, 80.f);
            window.draw(title);

            sf::Text guide;
            guide.setFont(font);
            guide.setCharacterSize(24);
            guide.setFillColor(sf::Color::White);
            guide.setString(
                "Controls:\n"
                " - Move: Arrow Keys / A D\n"
                " - Shoot: Space\n\n"
                "Items:\n"
                " - DMG+ : Increases bullet damage\n"
                " - SINGLE: Fire 1 bullet\n"
                " - DOUBLE: Fire 2 bullets\n"
                " - SPREAD: Fire 3 bullets\n\n"
                "Press ESC to return to Pause the game.");
            guide.setPosition(WINDOW_WIDTH / 2.f - guide.getGlobalBounds().width / 2.f, 200.f);
            window.draw(guide);
        }
        else if (currentState == SAVE_SLOTS)
        {
            window.clear(sf::Color(30, 30, 50));
            sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

            updateButtonAppearance(slot1, mousePos);
            updateButtonAppearance(slot2, mousePos);
            updateButtonAppearance(slot3, mousePos);
            updateButtonAppearance(backButton, mousePos);

            // Click handling
            if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left)
            {
                if (slot1.isHovered(mousePos))
                {
                    if (prevState == PLAYING)
                    {
                        saveGame(player, level, score, style, enemies, items, 1);
                        currentState = PLAYING;
                    }
                    else
                    { // LOAD
                        if (loadGame(player, level, score, style, enemies, items, pBullets, eBullets, 1))
                        {
                            for (int i = 0; i < MAX_ITEMS; ++i)
                            {
                                if (items[i].active)
                                {
                                    items[i].text.setFont(font);
                                    items[i].text.setCharacterSize(20);
                                    auto b = items[i].text.getLocalBounds();
                                    items[i].text.setOrigin(b.left + b.width / 2.f, b.top + b.height / 2.f);
                                    items[i].text.setPosition(items[i].pos);
                                    switch (items[i].type)
                                    {
                                    case ITEM_DMG:
                                        items[i].text.setString("DMG+1");
                                        items[i].text.setFillColor(sf::Color::Green);
                                        break;
                                    case ITEM_SINGLE:
                                        items[i].text.setString("Single");
                                        items[i].text.setFillColor(sf::Color::Cyan);
                                        break;
                                    case ITEM_DOUBLE:
                                        items[i].text.setString("Double");
                                        items[i].text.setFillColor(sf::Color::Magenta);
                                        break;
                                    case ITEM_SPREAD:
                                        items[i].text.setString("Spread");
                                        items[i].text.setFillColor(sf::Color::Yellow);
                                        break;
                                    default:
                                        break;
                                    }
                                }
                            }
                            currentState = PLAYING;
                        }
                    }
                }
                else if (slot2.isHovered(mousePos))
                {
                    if (prevState == PLAYING)
                    {
                        saveGame(player, level, score, style, enemies, items, 2);
                        currentState = PLAYING;
                    }
                    else
                    {
                        if (loadGame(player, level, score, style, enemies, items, pBullets, eBullets, 2))
                        {
                            for (int i = 0; i < MAX_ITEMS; ++i)
                            {
                                if (items[i].active)
                                {
                                    items[i].text.setFont(font);
                                    items[i].text.setCharacterSize(20);
                                    auto b = items[i].text.getLocalBounds();
                                    items[i].text.setOrigin(b.left + b.width / 2.f, b.top + b.height / 2.f);
                                    items[i].text.setPosition(items[i].pos);
                                    switch (items[i].type)
                                    {
                                    case ITEM_DMG:
                                        items[i].text.setString("DMG+1");
                                        items[i].text.setFillColor(sf::Color::Green);
                                        break;
                                    case ITEM_SINGLE:
                                        items[i].text.setString("Single");
                                        items[i].text.setFillColor(sf::Color::Cyan);
                                        break;
                                    case ITEM_DOUBLE:
                                        items[i].text.setString("Double");
                                        items[i].text.setFillColor(sf::Color::Magenta);
                                        break;
                                    case ITEM_SPREAD:
                                        items[i].text.setString("Spread");
                                        items[i].text.setFillColor(sf::Color::Yellow);
                                        break;
                                    default:
                                        break;
                                    }
                                }
                            }
                            currentState = PLAYING;
                        }
                    }
                }
                else if (slot3.isHovered(mousePos))
                {
                    if (prevState == PLAYING)
                    {
                        saveGame(player, level, score, style, enemies, items, 3);
                        currentState = PLAYING;
                    }
                    else
                    {
                        if (loadGame(player, level, score, style, enemies, items, pBullets, eBullets, 3))
                        {
                            for (int i = 0; i < MAX_ITEMS; ++i)
                            {
                                if (items[i].active)
                                {
                                    items[i].text.setFont(font);
                                    items[i].text.setCharacterSize(20);
                                    auto b = items[i].text.getLocalBounds();
                                    items[i].text.setOrigin(b.left + b.width / 2.f, b.top + b.height / 2.f);
                                    items[i].text.setPosition(items[i].pos);
                                    switch (items[i].type)
                                    {
                                    case ITEM_DMG:
                                        items[i].text.setString("DMG+1");
                                        items[i].text.setFillColor(sf::Color::Green);
                                        break;
                                    case ITEM_SINGLE:
                                        items[i].text.setString("Single");
                                        items[i].text.setFillColor(sf::Color::Cyan);
                                        break;
                                    case ITEM_DOUBLE:
                                        items[i].text.setString("Double");
                                        items[i].text.setFillColor(sf::Color::Magenta);
                                        break;
                                    case ITEM_SPREAD:
                                        items[i].text.setString("Spread");
                                        items[i].text.setFillColor(sf::Color::Yellow);
                                        break;
                                    default:
                                        break;
                                    }
                                }
                            }
                            currentState = PLAYING;
                        }
                    }
                }
                else if (backButton.isHovered(mousePos))
                {
                    currentState = prevState;
                }
            }
            // draw UI
            window.draw(slot1.rect);
            window.draw(slot1.text);
            window.draw(slot2.rect);
            window.draw(slot2.text);
            window.draw(slot3.rect);
            window.draw(slot3.text);
            window.draw(backButton.rect);
            window.draw(backButton.text);
        }
        else if (currentState == HIGH_SCORES)
        {
            window.clear(sf::Color(20, 20, 40));

            sf::Text title("HIGH SCORES", font, 60);
            title.setFillColor(sf::Color::Yellow);
            title.setPosition(WINDOW_WIDTH / 2.f - title.getGlobalBounds().width / 2.f, 80.f);
            window.draw(title);

            std::map<GameMode, std::vector<HighScoreEntry>> allScores;
            loadHighScores(allScores);

            const auto &scores = allScores[currentMode]; // bảng của mode hiện tại

            float y = 180.f;
            for (size_t i = 0; i < scores.size(); i++)
            {
                sf::Text entry;
                entry.setFont(font);
                entry.setCharacterSize(32);
                entry.setFillColor(sf::Color::White);
                entry.setString(std::to_string(i + 1) + ". Score: " + std::to_string(scores[i].score) +
                                "  Level: " + std::to_string(scores[i].level));
                entry.setPosition(WINDOW_WIDTH / 2.f - entry.getGlobalBounds().width / 2.f, y);
                y += 50.f;
                window.draw(entry);
            }

            // Back button
            static Button backButton;
            static bool inited = false;
            if (!inited)
            {
                setupButton(backButton, "Back", {WINDOW_WIDTH / 2.f, WINDOW_HEIGHT - 120.f});
                inited = true;
            }

            updateButtonAppearance(backButton, window.mapPixelToCoords(sf::Mouse::getPosition(window)));
            window.draw(backButton.rect);
            window.draw(backButton.text);

            if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left)
            {
                if (backButton.isHovered(window.mapPixelToCoords({event.mouseButton.x, event.mouseButton.y})))
                {
                    currentState = MENU;
                }
            }
        }
        else if (currentState == MODE)
        {
            window.clear(sf::Color(15, 15, 30));

            sf::Text title("Select Mode", font, 60);
            title.setFillColor(sf::Color::Cyan);
            title.setPosition(WINDOW_WIDTH / 2.f - title.getGlobalBounds().width / 2.f, 80.f);
            window.draw(title);

            sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
            updateButtonAppearance(normalModeButton, mousePos);
            updateButtonAppearance(hardModeButton, mousePos);
            updateButtonAppearance(survivalModeButton, mousePos);
            updateButtonAppearance(modeBackButton, mousePos);

            window.draw(normalModeButton.rect);
            window.draw(normalModeButton.text);
            window.draw(hardModeButton.rect);
            window.draw(hardModeButton.text);
            window.draw(survivalModeButton.rect);
            window.draw(survivalModeButton.text);
            window.draw(modeBackButton.rect);
            window.draw(modeBackButton.text);

            if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left)
            {
                if (normalModeButton.isHovered(mousePos))
                {
                    currentMode = MODE_NORMAL;
                    currentState = MENU;
                }
                else if (hardModeButton.isHovered(mousePos))
                {
                    currentMode = MODE_HARD;
                    currentState = MENU;
                }
                else if (survivalModeButton.isHovered(mousePos))
                {
                    currentMode = MODE_SURVIVAL;
                    currentState = MENU;
                }
                else if (modeBackButton.isHovered(mousePos))
                {
                    currentState = MENU;
                }
            }
        }

        window.display();
    }
    return 0;
}

// Menu (done)
// Game Over (done)
// Score (done)
// Pause (done)
// Shooting Styles (done)
// Help (done)
// Boss Phases (done)
// Save slot (done)
// High Scores (done)
// Mode (done)
// Upgrade UI
// Sound
