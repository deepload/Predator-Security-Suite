#include "../predator_i.h"
#include "../helpers/predator_crypto_calypso.h"
#include <gui/elements.h>

// Calypso Contracts Viewer - Display all active contracts
// Pattern: Scrollable list with detailed info

typedef struct {
    CalypsoCard card;
    CalypsoContract contracts[4];
    uint32_t contract_count;
    uint32_t selected_index;
    char status_text[64];
} ContractsState;

static ContractsState* state = NULL;

static void contracts_draw_callback(Canvas* canvas, void* context) {
    PredatorApp* app = context;
    if(!state) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Contracts");

    if(state->contract_count == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 30, "No contracts found");
        return;
    }

    canvas_set_font(canvas, FontSecondary);
    
    // Show selected contract in detail
    CalypsoContract* contract = &state->contracts[state->selected_index];
    
    // Contract number and status
    char header[64];
    const char* status_icon = contract->is_active ? "✓" : "✗";
    snprintf(header, sizeof(header),
             "%s Contract #%u",
             status_icon,
             contract->contract_number);
    canvas_draw_str(canvas, 2, 20, header);
    
    // Status
    canvas_draw_str(canvas, 2, 29, contract->is_active ? "Active" : "Inactive");
    
    // Validity period
    char valid_start[64];
    snprintf(valid_start, sizeof(valid_start),
             "Valid from: %02X/%02X/%02X",
             contract->validity_start[2],
             contract->validity_start[1],
             contract->validity_start[0]);
    canvas_draw_str(canvas, 2, 38, valid_start);
    
    char valid_end[64];
    snprintf(valid_end, sizeof(valid_end),
             "Valid until: %02X/%02X/%02X",
             contract->validity_end[2],
             contract->validity_end[1],
             contract->validity_end[0]);
    canvas_draw_str(canvas, 2, 46, valid_end);
    
    // Tariff code
    char tariff[64];
    snprintf(tariff, sizeof(tariff),
             "Tariff: %02X%02X",
             contract->tariff[0],
             contract->tariff[1]);
    canvas_draw_str(canvas, 2, 54, tariff);
    
    // Contract selector
    if(state->contract_count > 1) {
        char selector[32];
        snprintf(selector, sizeof(selector),
                 "◀ %lu/%lu ▶",
                 state->selected_index + 1,
                 state->contract_count);
        canvas_draw_str(canvas, 80, 10, selector);
    }
    
    // Status bar
    canvas_draw_line(canvas, 0, 60, 128, 60);
    canvas_draw_str(canvas, 2, 63, state->status_text);
}

static bool contracts_input_callback(InputEvent* event, void* context) {
    PredatorApp* app = context;
    
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
        
        // Navigate between contracts
        if(event->key == InputKeyLeft && state->selected_index > 0) {
            state->selected_index--;
            view_port_update(app->view_port);
            return true;
        }
        
        if(event->key == InputKeyRight && 
           state->selected_index < state->contract_count - 1) {
            state->selected_index++;
            view_port_update(app->view_port);
            return true;
        }
    }
    
    return false;
}

void predator_scene_calypso_contracts_on_enter(void* context) {
    PredatorApp* app = context;
    
    // Allocate state
    state = malloc(sizeof(ContractsState));
    memset(state, 0, sizeof(ContractsState));
    
    // Read contracts
    // Real implementation would call:
    // state->contract_count = calypso_read_all_contracts(
    //     app, &state->card, state->contracts, 4);
    
    if(state->contract_count > 1) {
        snprintf(state->status_text, sizeof(state->status_text),
                 "◀/▶ Switch, Back to exit");
    } else {
        snprintf(state->status_text, sizeof(state->status_text),
                 "Back to exit");
    }
    
    // Setup view port
    view_port_draw_callback_set(app->view_port, contracts_draw_callback, app);
    view_port_input_callback_set(app->view_port, contracts_input_callback, app);
    
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
}

bool predator_scene_calypso_contracts_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void predator_scene_calypso_contracts_on_exit(void* context) {
    PredatorApp* app = context;
    
    gui_remove_view_port(app->gui, app->view_port);
    
    if(state) {
        free(state);
        state = NULL;
    }
}
