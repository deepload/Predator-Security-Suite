#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * Calypso Contactless Card Standard
 * 
 * European standard for public transport ticketing.
 * Interoperable system used across multiple countries.
 * 
 * Major implementations:
 * - France: Navigo (Paris Metro - 4.5M daily users), Lyon, Marseille
 * - Belgium: Brussels MOBIB
 * - Portugal: Lisbon Viva Viagem, Porto Andante
 * - Greece: Athens ATH.ENA
 * - Tunisia: Tunis rapid transit
 * 
 * ~50 million cards in circulation
 * Growing adoption in Eastern Europe
 * 
 * CRITICAL: Obtain written authorization before testing any Calypso systems.
 */

// Forward declare app
struct PredatorApp;

// Calypso operates on ISO 14443 Type B at 13.56 MHz
// Uses proprietary cryptographic protocol with session keys

// Card types
typedef enum {
    Calypso_Unknown,
    Calypso_Navigo,              // Paris Navigo (RATP)
    Calypso_Lyon_TCL,            // Lyon TCL
    Calypso_MOBIB,               // Brussels STIB/MIVB
    Calypso_VivaViagem,          // Lisbon transit
    Calypso_Andante,             // Porto metro
    Calypso_Athens,              // Athens metro
    Calypso_Generic              // Generic Calypso implementation
} CalypsoCardType;

// Calypso revisions
typedef enum {
    Calypso_Rev1,                // Original (being phased out)
    Calypso_Rev2,                // Current standard
    Calypso_Rev3,                // Latest (AES-128)
    Calypso_Rev3_Light           // Lightweight variant
} CalypsoRevision;

// Security levels
typedef enum {
    Calypso_Security_None,       // No security (rare)
    Calypso_Security_DES,        // DES (Rev1, legacy)
    Calypso_Security_3DES,       // 3DES (Rev2, most common)
    Calypso_Security_AES128      // AES-128 (Rev3, new cards)
} CalypsoSecurityLevel;

// File types (similar to DESFire)
typedef enum {
    Calypso_File_Binary,         // Binary file
    Calypso_File_Linear,         // Linear record file
    Calypso_File_Cyclic,         // Cyclic record file
    Calypso_File_Counter         // Counter file (for trips)
} CalypsoFileType;

// Card structure
typedef struct {
    uint8_t uid[4];              // ISO 14443 Type B UID (4 bytes)
    uint8_t atr[32];             // Answer To Reset
    uint8_t atr_len;
    uint32_t card_number;        // Logical card number
    CalypsoCardType card_type;
    CalypsoRevision revision;
    CalypsoSecurityLevel security;
    bool authenticated;
} CalypsoCard;

// Application structure (Calypso uses applications like DESFire)
typedef struct {
    uint8_t application_id;      // Application ID
    uint8_t key_index;           // Key index for this application
    uint8_t file_list[32];       // File IDs in this application
    uint8_t file_count;
    bool is_selected;
} CalypsoApplication;

// Authentication context
typedef struct {
    uint8_t issuer_key[16];      // Issuer key (3DES or AES)
    uint8_t session_key[16];     // Session key after authentication
    uint8_t diversifier[8];      // Key diversifier (usually based on card number)
    uint8_t challenge[8];        // Random challenge
    uint8_t key_index;           // Key index being used
    CalypsoSecurityLevel security;
    bool authenticated;
} CalypsoAuthContext;

// Contract structure (transit subscription/ticket)
typedef struct {
    uint8_t contract_number;     // Contract ID on card
    uint8_t tariff_code;         // Tariff/pricing code
    uint16_t profile_number;     // User profile
    uint8_t validity_start[3];   // Start date (YYMMDD)
    uint8_t validity_end[3];     // End date (YYMMDD)
    uint16_t trip_counter;       // Remaining trips
    uint16_t minutes_remaining;  // Remaining time (for time-based tickets)
    uint8_t zones[8];            // Valid zones (bitmask)
    bool is_active;
} CalypsoContract;

// Event log entry (journey history)
typedef struct {
    uint8_t event_type;          // 0x01=entry, 0x02=exit, 0x03=inspection
    uint8_t date[3];             // Date (YYMMDD)
    uint8_t time[2];             // Time (HHMM)
    uint16_t location_id;        // Station/Stop ID
    uint8_t contract_used;       // Which contract was used
    uint16_t balance_after;      // Balance after transaction (if applicable)
    uint8_t vehicle_id[2];       // Bus/Train ID
} CalypsoEvent;

// ========== Detection & Selection ==========

/**
 * Detect Calypso card
 * @param app PredatorApp context
 * @param card Output card structure
 * @return true if Calypso card detected
 */
bool calypso_detect_card(struct PredatorApp* app, CalypsoCard* card);

/**
 * Select Calypso application
 * @param app PredatorApp context
 * @param card Card structure
 * @param application_id Application to select (usually 0x01)
 * @return true if successful
 */
bool calypso_select_application(struct PredatorApp* app, const CalypsoCard* card,
                                uint8_t application_id);

/**
 * Get card serial number
 * @param app PredatorApp context
 * @param card Card structure
 * @param serial_number Output serial number (8 bytes)
 * @return true if successful
 */
bool calypso_get_serial_number(struct PredatorApp* app, const CalypsoCard* card,
                               uint8_t* serial_number);

/**
 * Identify card type from ATR and application data
 * @param card Card structure
 * @return Detected card type
 */
CalypsoCardType calypso_identify_card(const CalypsoCard* card);

// ========== Authentication ==========

/**
 * Open secure session (authenticate)
 * @param app PredatorApp context
 * @param card Card structure
 * @param auth_ctx Authentication context with issuer key
 * @param key_index Key index to use (1-3 typically)
 * @return true if authenticated
 */
bool calypso_open_secure_session(struct PredatorApp* app, const CalypsoCard* card,
                                 CalypsoAuthContext* auth_ctx, uint8_t key_index);

/**
 * Close secure session (finalize transaction)
 * @param app PredatorApp context
 * @param auth_ctx Authenticated context
 * @return true if successful
 */
bool calypso_close_secure_session(struct PredatorApp* app, CalypsoAuthContext* auth_ctx);

/**
 * Generate session key from authentication
 * @param auth_ctx Authentication context
 * @param card_challenge Challenge from card
 * @param reader_challenge Challenge from reader
 * @return true if successful
 */
bool calypso_generate_session_key(CalypsoAuthContext* auth_ctx,
                                  const uint8_t* card_challenge,
                                  const uint8_t* reader_challenge);

/**
 * Diversify issuer key (derive card-specific key)
 * @param master_key Master issuer key
 * @param diversifier Diversifier (usually card number)
 * @param diversified_key Output diversified key
 * @return true if successful
 */
bool calypso_diversify_key(const uint8_t* master_key, const uint8_t* diversifier,
                           uint8_t* diversified_key);

// ========== Read Operations ==========

/**
 * Read record (for linear/cyclic files)
 * @param app PredatorApp context
 * @param card Card structure
 * @param file_id File ID
 * @param record_number Record number
 * @param data Output buffer
 * @param max_len Maximum length
 * @return Number of bytes read
 */
uint32_t calypso_read_record(struct PredatorApp* app, const CalypsoCard* card,
                             uint8_t file_id, uint8_t record_number,
                             uint8_t* data, uint32_t max_len);

/**
 * Read binary file
 * @param app PredatorApp context
 * @param card Card structure
 * @param file_id File ID
 * @param offset Offset in file
 * @param length Length to read
 * @param data Output buffer
 * @return Number of bytes read
 */
uint32_t calypso_read_binary(struct PredatorApp* app, const CalypsoCard* card,
                             uint8_t file_id, uint16_t offset, uint16_t length,
                             uint8_t* data);

/**
 * Read contract data (ticket/subscription)
 * @param app PredatorApp context
 * @param card Card structure
 * @param contract_number Contract number (1-4 typically)
 * @param contract Output contract structure
 * @return true if successful
 */
bool calypso_read_contract(struct PredatorApp* app, const CalypsoCard* card,
                           uint8_t contract_number, CalypsoContract* contract);

/**
 * Read all contracts on card
 * @param app PredatorApp context
 * @param card Card structure
 * @param contracts Output contract array
 * @param max_contracts Maximum contracts
 * @return Number of contracts read
 */
uint32_t calypso_read_all_contracts(struct PredatorApp* app, const CalypsoCard* card,
                                    CalypsoContract* contracts, uint32_t max_contracts);

/**
 * Read event log (journey history)
 * @param app PredatorApp context
 * @param card Card structure
 * @param events Output event array
 * @param max_events Maximum events
 * @return Number of events read
 */
uint32_t calypso_read_event_log(struct PredatorApp* app, const CalypsoCard* card,
                                CalypsoEvent* events, uint32_t max_events);

/**
 * Read balance/counter
 * @param app PredatorApp context
 * @param card Card structure
 * @param counter_number Counter number
 * @param balance Output balance
 * @return true if successful
 */
bool calypso_read_counter(struct PredatorApp* app, const CalypsoCard* card,
                          uint8_t counter_number, uint16_t* balance);

// ========== Write Operations (requires authentication) ==========

/**
 * Update record (requires secure session)
 * @param app PredatorApp context
 * @param auth_ctx Authenticated context
 * @param file_id File ID
 * @param record_number Record number
 * @param data Data to write
 * @param len Data length
 * @return true if successful
 */
bool calypso_update_record(struct PredatorApp* app, const CalypsoAuthContext* auth_ctx,
                           uint8_t file_id, uint8_t record_number,
                           const uint8_t* data, uint32_t len);

/**
 * Increase counter (add value)
 * @param app PredatorApp context
 * @param auth_ctx Authenticated context
 * @param counter_number Counter number
 * @param amount Amount to add
 * @return true if successful
 */
bool calypso_increase_counter(struct PredatorApp* app, const CalypsoAuthContext* auth_ctx,
                              uint8_t counter_number, uint16_t amount);

/**
 * Decrease counter (subtract value, for trip counting)
 * @param app PredatorApp context
 * @param auth_ctx Authenticated context
 * @param counter_number Counter number
 * @param amount Amount to subtract
 * @return true if successful
 */
bool calypso_decrease_counter(struct PredatorApp* app, const CalypsoAuthContext* auth_ctx,
                              uint8_t counter_number, uint16_t amount);

// ========== Security Research ==========

/**
 * Dictionary attack on issuer key
 * @param app PredatorApp context
 * @param card Card structure
 * @param key_index Key index to attack
 * @param found_key Output key if found
 * @return true if key found
 */
bool calypso_attack_dictionary(struct PredatorApp* app, const CalypsoCard* card,
                               uint8_t key_index, uint8_t* found_key);

/**
 * Analyze card security features
 * @param app PredatorApp context
 * @param card Card structure
 * @param report Output security report (min 512 bytes)
 * @return true if successful
 */
bool calypso_analyze_security(struct PredatorApp* app, const CalypsoCard* card,
                              char* report);

/**
 * Dump entire card (all readable data)
 * @param app PredatorApp context
 * @param card Card structure
 * @param output_path Path to save dump
 * @return true if successful
 */
bool calypso_dump_card(struct PredatorApp* app, const CalypsoCard* card,
                      const char* output_path);

// ========== Utilities ==========

/**
 * Calculate CRC for Calypso
 * @param data Data buffer
 * @param len Data length
 * @return CRC value
 */
uint16_t calypso_crc(const uint8_t* data, uint32_t len);

/**
 * Parse contract data
 * @param raw_data Raw contract data (29 bytes typical)
 * @param contract Output contract structure
 * @param card_type Card type (for format interpretation)
 * @return true if successful
 */
bool calypso_parse_contract(const uint8_t* raw_data, CalypsoContract* contract,
                            CalypsoCardType card_type);

/**
 * Parse event log entry
 * @param raw_data Raw event data (29 bytes typical)
 * @param event Output event structure
 * @param card_type Card type
 * @return true if successful
 */
bool calypso_parse_event(const uint8_t* raw_data, CalypsoEvent* event,
                         CalypsoCardType card_type);

/**
 * Format contract for display
 * @param contract Contract structure
 * @param output Output string buffer (min 256 chars)
 * @param card_type Card type
 */
void calypso_format_contract(const CalypsoContract* contract, char* output,
                             CalypsoCardType card_type);

/**
 * Format event for display
 * @param event Event structure
 * @param output Output string buffer (min 128 chars)
 * @param card_type Card type
 */
void calypso_format_event(const CalypsoEvent* event, char* output,
                          CalypsoCardType card_type);

/**
 * Decode station ID (Navigo/Paris specific)
 * @param location_id Location ID
 * @param station_name Output station name
 * @param max_len Maximum length
 * @return true if station found in database
 */
bool calypso_decode_navigo_station(uint16_t location_id, char* station_name,
                                   size_t max_len);

/**
 * Get card type name
 * @param type Card type
 * @return Human-readable name
 */
const char* calypso_get_card_name(CalypsoCardType type);

// ========== Cryptographic Functions ==========

/**
 * 3DES encryption for Calypso (Rev2)
 * @param key 3DES key (16 bytes, 2-key)
 * @param data Data to encrypt (8 bytes)
 * @param output Output buffer (8 bytes)
 * @return true if successful
 */
bool calypso_3des_encrypt(const uint8_t* key, const uint8_t* data, uint8_t* output);

/**
 * 3DES decryption
 * @param key 3DES key (16 bytes)
 * @param data Data to decrypt (8 bytes)
 * @param output Output buffer (8 bytes)
 * @return true if successful
 */
bool calypso_3des_decrypt(const uint8_t* key, const uint8_t* data, uint8_t* output);

/**
 * AES-128 encryption for Calypso Rev3
 * @param key AES key (16 bytes)
 * @param data Data to encrypt (16 bytes)
 * @param output Output buffer (16 bytes)
 * @return true if successful
 */
bool calypso_aes_encrypt(const uint8_t* key, const uint8_t* data, uint8_t* output);

// ========== Known Keys & Defaults ==========

// Calypso uses diversified keys, but some test/default keys are known

extern const uint8_t CALYPSO_KEY_DEFAULT_3DES[16];
extern const uint8_t CALYPSO_KEY_DEFAULT_AES[16];
extern const uint8_t CALYPSO_KEY_NAVIGO_SAMPLE[16];  // Sample key (non-production)

/**
 * Load common issuer keys for dictionary attack
 * @param keys Output key array
 * @param max_keys Maximum keys
 * @return Number of keys loaded
 */
uint32_t calypso_load_common_keys(uint8_t keys[][16], uint32_t max_keys);

/**
 * AUTHORIZATION WARNING:
 * 
 * Calypso attacks must ONLY be used on cards you own or have
 * explicit written authorization to test.
 * 
 * ILLEGAL USE CASES (DO NOT):
 * - Transit fare evasion
 * - Unauthorized ticket manipulation
 * - Balance/counter fraud
 * - Identity theft or impersonation
 * - Any form of fraud or unauthorized access
 * 
 * LEGAL USE CASES (AUTHORIZED ONLY):
 * - Testing your own Calypso cards
 * - Authorized security research for transit authorities
 * - Academic research in controlled environments
 * - Professional penetration testing with written contracts
 * 
 * Unauthorized Calypso manipulation is a SERIOUS CRIME in Europe.
 * 
 * France: Fraud and fare evasion laws carry heavy penalties
 * - Fare evasion: Up to €375 fine
 * - Card fraud: Up to 5 years imprisonment + €375,000 fine
 * 
 * Belgium, Portugal: Similar fraud prevention laws with severe penalties
 * 
 * Transit fraud is prosecuted vigorously across Europe.
 */
