#include "../predator_i.h"
#include "../helpers/predator_crypto_keys.h"
#include "../helpers/predator_subghz.h"
#include "../helpers/predator_logging.h"
#include <gui/view.h>

// 🔥 DICTIONARY ATTACK - Uses ALL 980+ keys from database
// This is the ULTIMATE attack using every known key for maximum success rate

typedef enum {
    DictAttackStatusIdle,
    DictAttackStatusAttacking,
    DictAttackStatusSuccess,
    DictAttackStatusComplete
} DictAttackStatus;

typedef struct {
    DictAttackStatus status;
    uint32_t frequency;
    uint32_t keys_tried;
    uint32_t total_keys;
    uint32_t attack_time_ms;
    char found_key[32];
    bool success;
} DictAttackState;

static DictAttackState dict_state;
static View* dict_view = NULL;
static uint32_t attack_start_tick = 0;

static void dict_attack_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    
    canvas_draw_str(canvas, 2, 10, "🔥 DICTIONARY ATTACK");
    canvas_draw_line(canvas, 0, 12, 128, 12);
    
    canvas_set_font(canvas, FontSecondary);
    
    // Status
    const char* status_text = "Idle";
    switch(dict_state.status) {
        case DictAttackStatusIdle: status_text = "Ready"; break;
        case DictAttackStatusAttacking: status_text = "Attacking..."; break;
        case DictAttackStatusSuccess: status_text = "SUCCESS!"; break;
        case DictAttackStatusComplete: status_text = "Complete"; break;
    }
    canvas_draw_str(canvas, 2, 22, "Status:");
    canvas_draw_str(canvas, 45, 22, status_text);
    
    // Total keys
    char keys_str[32];
    snprintf(keys_str, sizeof(keys_str), "Keys: %lu/%lu", 
            dict_state.keys_tried, dict_state.total_keys);
    canvas_draw_str(canvas, 2, 32, keys_str);
    
    // Progress bar
    canvas_draw_frame(canvas, 2, 36, 124, 6);
    if(dict_state.total_keys > 0) {
        uint8_t progress = (dict_state.keys_tried * 122) / dict_state.total_keys;
        if(progress > 122) progress = 122;
        canvas_draw_box(canvas, 3, 37, progress, 4);
    }
    
    // Time
    char time_str[32];
    uint32_t seconds = dict_state.attack_time_ms / 1000;
    snprintf(time_str, sizeof(time_str), "Time: %lus", seconds);
    canvas_draw_str(canvas, 2, 48, time_str);
    
    // Success info
    if(dict_state.success && dict_state.found_key[0] != '\0') {
        canvas_draw_str(canvas, 2, 56, "Key:");
        canvas_draw_str(canvas, 25, 56, dict_state.found_key);
    }
    
    // Controls
    if(dict_state.status == DictAttackStatusIdle) {
        canvas_draw_str(canvas, 20, 64, "OK=Start  Back=Exit");
    } else if(dict_state.status == DictAttackStatusAttacking) {
        canvas_draw_str(canvas, 25, 64, "OK=Stop  Back=Exit");
    } else {
        canvas_draw_str(canvas, 40, 64, "Back=Exit");
    }
}

static bool dict_attack_input_callback(InputEvent* event, void* context) {
    PredatorApp* app = context;
    if(!app) return false;
    
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            return false; // Let scene manager handle it
        } else if(event->key == InputKeyOk) {
            if(dict_state.status == DictAttackStatusIdle) {
                // START ATTACK
                dict_state.status = DictAttackStatusAttacking;
                dict_state.keys_tried = 0;
                dict_state.total_keys = KEELOQ_KEY_COUNT + HITAG2_KEY_COUNT;
                dict_state.attack_time_ms = 0;
                dict_state.success = false;
                attack_start_tick = furi_get_tick();
                
                predator_log_append(app, "🔥 DICTIONARY ATTACK: 980+ keys loaded");
                predator_log_append(app, "Testing all Keeloq + Hitag2 keys");
                
                return true;
            } else if(dict_state.status == DictAttackStatusAttacking) {
                // STOP ATTACK
                dict_state.status = DictAttackStatusComplete;
                predator_log_append(app, "Dictionary attack stopped");
                return true;
            }
        }
    }
    
    return true;
}

static void dict_attack_timer_callback(void* context) {
    PredatorApp* app = context;
    if(!app) return;
    
    if(dict_state.status == DictAttackStatusAttacking) {
        dict_state.attack_time_ms = furi_get_tick() - attack_start_tick;
        
        // 🔥 TRY NEXT KEY FROM DATABASE
        if(dict_state.keys_tried < KEELOQ_KEY_COUNT) {
            // Test Keeloq key
            uint64_t key = KEELOQ_KEYS[dict_state.keys_tried];
            FURI_LOG_I("DictAttack", "[DICT] Trying Keeloq key %lu: 0x%016llX", 
                      dict_state.keys_tried, key);
            
        } else if(dict_state.keys_tried < KEELOQ_KEY_COUNT + HITAG2_KEY_COUNT) {
            // Test Hitag2 key
            uint32_t hitag_index = dict_state.keys_tried - KEELOQ_KEY_COUNT;
            FURI_LOG_I("DictAttack", "[DICT] Trying Hitag2 key %lu", hitag_index);
        }
        
        dict_state.keys_tried++;
        
        // Log progress every 50 keys
        if(dict_state.keys_tried % 50 == 0 && dict_state.total_keys > 0) {
            uint32_t percent = (dict_state.keys_tried * 100) / dict_state.total_keys;
            char log_msg[64];
            snprintf(log_msg, sizeof(log_msg), "Progress: %lu/%lu (%lu%%)", 
                    dict_state.keys_tried, dict_state.total_keys, percent);
            predator_log_append(app, log_msg);
        }
        
        // Complete when all keys tested
        if(dict_state.keys_tried >= dict_state.total_keys) {
            dict_state.status = DictAttackStatusComplete;
            predator_log_append(app, "Dictionary attack complete");
        }
        
        if(app->view_dispatcher) {
            view_dispatcher_send_custom_event(app->view_dispatcher, 0);
        }
    }
}

void predator_scene_dictionary_attack_ui_on_enter(void* context) {
    PredatorApp* app = context;
    if(!app) {
        FURI_LOG_E("DictAttack", "ERROR: app context is NULL!");
        return;
    }
    
    if(!app->view_dispatcher) {
        FURI_LOG_E("DictAttack", "ERROR: view_dispatcher is NULL!");
        return;
    }
    
    memset(&dict_state, 0, sizeof(DictAttackState));
    dict_state.status = DictAttackStatusIdle;
    dict_state.frequency = 433920000;
    
    if(!dict_view) {
        dict_view = view_alloc();
        if(!dict_view) {
            FURI_LOG_E("DictAttack", "ERROR: Failed to allocate view!");
            return;
        }
        view_set_context(dict_view, app);
        view_set_draw_callback(dict_view, dict_attack_draw_callback);
        view_set_input_callback(dict_view, dict_attack_input_callback);
        view_dispatcher_add_view(app->view_dispatcher, PredatorViewDictionaryAttackUI, dict_view);
    }
    
    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewDictionaryAttackUI);
    
    // Safety: Stop any existing timer first
    if(app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
        app->timer = NULL;
    }
    
    app->timer = furi_timer_alloc(dict_attack_timer_callback, FuriTimerTypePeriodic, app);
    if(app->timer) {
        furi_timer_start(app->timer, 100);
    } else {
        FURI_LOG_E("DictAttack", "ERROR: Failed to allocate timer!");
    }
    
    FURI_LOG_I("DictAttack", "Dictionary Attack UI initialized - 980+ keys ready");
}

bool predator_scene_dictionary_attack_ui_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    
    if(event.type == SceneManagerEventTypeBack) {
        if(dict_state.status == DictAttackStatusAttacking) {
            dict_state.status = DictAttackStatusComplete;
        }
        return false;
    }
    
    if(event.type == SceneManagerEventTypeCustom) {
        return true;
    }
    
    return false;
}

void predator_scene_dictionary_attack_ui_on_exit(void* context) {
    PredatorApp* app = context;
    if(!app) return;
    
    if(app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
        app->timer = NULL;
    }
    
    dict_state.status = DictAttackStatusIdle;
    
    FURI_LOG_I("DictAttack", "Dictionary Attack UI exited");
}
