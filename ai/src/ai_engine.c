/* SPDX-FileCopyrightText: 2025 Rhett Creighton
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ai/personality.h"
#include "ai/ai_player.h"
#include "poker/logger.h"
#include <stdlib.h>
#include <time.h>

// Global AI engine state
static struct {
    bool initialized;
    uint32_t num_ai_players;
    uint64_t random_seed;
} g_ai_engine = {
    .initialized = false,
    .num_ai_players = 0,
    .random_seed = 0
};

// Initialize AI engine
void ai_engine_init(void) {
    if (g_ai_engine.initialized) {
        return;
    }
    
    g_ai_engine.random_seed = time(NULL);
    srand(g_ai_engine.random_seed);
    g_ai_engine.initialized = true;
    
    LOG_AI_INFO("AI engine initialized with seed %lu", g_ai_engine.random_seed);
}

// Shutdown AI engine
void ai_engine_shutdown(void) {
    if (!g_ai_engine.initialized) {
        return;
    }
    
    g_ai_engine.initialized = false;
    g_ai_engine.num_ai_players = 0;
    
    LOG_AI_INFO("AI engine shutdown");
}

// Get random personality
AIPersonality ai_engine_get_random_personality(void) {
    if (!g_ai_engine.initialized) {
        ai_engine_init();
    }
    
    // Choose random type with weights
    int r = rand() % 100;
    
    if (r < 30) {
        // 30% tight aggressive (good players)
        return AI_PERSONALITY_TIGHT_AGGRESSIVE;
    } else if (r < 50) {
        // 20% loose passive (fish)
        return AI_PERSONALITY_LOOSE_PASSIVE;
    } else if (r < 70) {
        // 20% tight passive (rocks)
        return AI_PERSONALITY_TIGHT_PASSIVE;
    } else if (r < 85) {
        // 15% loose aggressive (maniacs)
        return AI_PERSONALITY_LOOSE_AGGRESSIVE;
    } else {
        // 15% truly random
        return AI_PERSONALITY_RANDOM;
    }
}

// Get personality by skill level
AIPersonality ai_engine_get_personality_by_skill(int skill_level) {
    if (!g_ai_engine.initialized) {
        ai_engine_init();
    }
    
    // Clamp skill level
    if (skill_level < 1) skill_level = 1;
    if (skill_level > 10) skill_level = 10;
    
    AIPersonality personality;
    
    if (skill_level < 3) {
        // Beginners - loose passive
        personality = AI_PERSONALITY_LOOSE_PASSIVE;
        personality.skill_level = skill_level;
        
        // Make them even worse
        personality.pot_odds_accuracy = 0.1f + skill_level * 0.1f;
        personality.hand_reading_skill = 0.05f + skill_level * 0.05f;
        personality.position_awareness = 0.1f;
        personality.bluff_frequency = 0.02f;
        
    } else if (skill_level < 5) {
        // Weak regulars - tight passive
        personality = AI_PERSONALITY_TIGHT_PASSIVE;
        personality.skill_level = skill_level;
        
        personality.pot_odds_accuracy = 0.3f + (skill_level - 3) * 0.15f;
        personality.hand_reading_skill = 0.2f + (skill_level - 3) * 0.1f;
        personality.position_awareness = 0.3f + (skill_level - 3) * 0.1f;
        
    } else if (skill_level < 8) {
        // Decent players - tight aggressive
        personality = AI_PERSONALITY_TIGHT_AGGRESSIVE;
        personality.skill_level = skill_level;
        
        // Adjust for skill within range
        float skill_factor = (skill_level - 5) / 3.0f;
        personality.pot_odds_accuracy = 0.6f + skill_factor * 0.2f;
        personality.hand_reading_skill = 0.5f + skill_factor * 0.3f;
        personality.position_awareness = 0.6f + skill_factor * 0.2f;
        personality.adaptation_rate = 0.1f + skill_factor * 0.1f;
        
    } else {
        // Expert players - advanced TAG with exploitative tendencies
        personality = AI_PERSONALITY_TIGHT_AGGRESSIVE;
        personality.skill_level = skill_level;
        personality.type = AI_TYPE_EXPLOITATIVE;
        
        // Near-optimal play
        personality.pot_odds_accuracy = 0.85f + (skill_level - 8) * 0.05f;
        personality.hand_reading_skill = 0.8f + (skill_level - 8) * 0.1f;
        personality.position_awareness = 0.9f;
        personality.adaptation_rate = 0.3f;
        personality.deception = 0.7f + (skill_level - 8) * 0.1f;
        personality.emotional_control = 0.9f + (skill_level - 8) * 0.05f;
        
        // Balanced frequencies
        personality.bluff_frequency = 0.25f; // Near GTO
        personality.three_bet = 0.08f + (skill_level - 8) * 0.02f;
        personality.steal_frequency = 0.35f;
    }
    
    return personality;
}

// Create AI player with skill level
AIPlayer* ai_engine_create_player(const char* name, int skill_level) {
    if (!g_ai_engine.initialized) {
        ai_engine_init();
    }
    
    // Get personality for skill level
    AIPersonality personality = ai_engine_get_personality_by_skill(skill_level);
    
    // Determine type from personality
    AIPlayerType type = personality.type;
    
    // Create player
    AIPlayer* player = ai_player_create(name, type);
    if (!player) {
        return NULL;
    }
    
    // Set the adjusted personality
    ai_player_set_personality(player, &personality);
    
    g_ai_engine.num_ai_players++;
    
    LOG_AI_INFO("Created AI player '%s' with skill level %d", 
                player->name, skill_level);
    
    return player;
}

// Create a table of AI players
AIPlayer** ai_engine_create_table(int num_players, int min_skill, int max_skill) {
    if (!g_ai_engine.initialized) {
        ai_engine_init();
    }
    
    if (num_players < 2 || num_players > 10) {
        LOG_AI_ERROR("Invalid number of players: %d", num_players);
        return NULL;
    }
    
    AIPlayer** table = calloc(num_players, sizeof(AIPlayer*));
    if (!table) {
        return NULL;
    }
    
    // Create players with varied skills
    for (int i = 0; i < num_players; i++) {
        char name[64];
        
        // Generate name
        const char* prefixes[] = {"Pro", "Shark", "Fish", "Rock", "Maniac", 
                                 "Grinder", "Whale", "Nit", "LAG", "TAG"};
        const char* suffixes[] = {"", "_Jr", "_Sr", "_III", "_2000", 
                                 "_XXX", "_007", "_Pro", "_$$$", "_King"};
        
        snprintf(name, sizeof(name), "%s%s", 
                prefixes[rand() % 10], suffixes[rand() % 10]);
        
        // Random skill in range
        int skill = min_skill + rand() % (max_skill - min_skill + 1);
        
        table[i] = ai_engine_create_player(name, skill);
        if (!table[i]) {
            // Clean up on failure
            for (int j = 0; j < i; j++) {
                ai_player_destroy(table[j]);
            }
            free(table);
            return NULL;
        }
    }
    
    LOG_AI_INFO("Created table with %d AI players (skills %d-%d)",
                num_players, min_skill, max_skill);
    
    return table;
}

// Destroy a table of AI players
void ai_engine_destroy_table(AIPlayer** table, int num_players) {
    if (!table) return;
    
    for (int i = 0; i < num_players; i++) {
        if (table[i]) {
            ai_player_destroy(table[i]);
            g_ai_engine.num_ai_players--;
        }
    }
    
    free(table);
}

// Get engine statistics
void ai_engine_get_stats(uint32_t* num_players, uint64_t* random_seed) {
    if (num_players) {
        *num_players = g_ai_engine.num_ai_players;
    }
    if (random_seed) {
        *random_seed = g_ai_engine.random_seed;
    }
}

// Set random seed for reproducible games
void ai_engine_set_seed(uint64_t seed) {
    g_ai_engine.random_seed = seed;
    srand(seed);
    
    LOG_AI_INFO("AI engine seed set to %lu", seed);
}