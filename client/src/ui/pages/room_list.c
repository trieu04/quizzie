#include "ui.h"
#include "net.h"
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>

static char room_id_input[16] = {0};
static int mode = 0;         // 0: room list, 1: manual input
static int selected_room = 0;
static char error_message[64] = {0};
static bool list_requested = false;

void page_room_list_draw(ClientContext* ctx) {
    int row, col;
    getmaxyx(stdscr, row, col);

    // Title
    attron(A_BOLD);
    if (mode == 0) {
        mvprintw(2, (col - 20)/2, "AVAILABLE ROOMS");
    } else {
        mvprintw(2, (col - 20)/2, "JOIN BY ROOM ID");
    }
    attroff(A_BOLD);
    
    // Status message
    if (strlen(ctx->status_message) > 0) {
        mvprintw(4, (col - strlen(ctx->status_message))/2, "%s", ctx->status_message);
    }

    if (mode == 0) {
        // Room list mode
        if (ctx->room_count == 0) {
            mvprintw(row/2 - 2, (col - 40)/2, "No rooms available. Press 'M' for manual input.");
        } else {
            // Table header
            int table_x = (col - 60) / 2;
            if (table_x < 2) table_x = 2;
            
            attron(A_BOLD);
            mvprintw(6, table_x, "%-6s %-20s %-10s %-12s %-10s", 
                "ID", "Host", "Players", "Status", "");
            attroff(A_BOLD);
            mvhline(7, table_x, ACS_HLINE, 60);
            
            // Room list
            int max_display = row - 14;
            if (max_display > ctx->room_count) max_display = ctx->room_count;
            
            for (int i = 0; i < max_display; i++) {
                RoomInfo* r = &ctx->rooms[i];
                
                if (i == selected_room) attron(A_REVERSE);
                
                const char* state_str = "Waiting";
                if (r->state == 1) state_str = "In Progress";
                else if (r->state == 2) state_str = "Finished";
                
                const char* host_marker = r->is_my_room ? "(You)" : "";
                
                mvprintw(8 + i, table_x, "%-6d %-20s %-10d %-12s %-10s", 
                    r->id, r->host_username, r->player_count, state_str, host_marker);
                
                if (i == selected_room) attroff(A_REVERSE);
            }
        }
        
        // Help text
        mvprintw(row - 4, 2, "UP/DOWN: Select | ENTER: Join Room | R: Refresh");
        mvprintw(row - 3, 2, "H: Rejoin as Host (if your room) | M: Manual input | ESC: Back");
    } else {
        // Manual input mode
        mvprintw(row/2 - 4, (col - 50)/2, "Enter the Room ID to join:");
        
        mvprintw(row/2 - 1, (col - 30)/2, "Room ID: ");
        attron(A_REVERSE);
        mvprintw(row/2 - 1, (col - 30)/2 + 10, "%-10s", room_id_input);
        attroff(A_REVERSE);
        
        mvprintw(row/2 + 2, (col - 30)/2, "Press ENTER to join, ESC to go back");
    }
    
    // Error message
    if (strlen(error_message) > 0) {
        attron(A_BOLD);
        mvprintw(row - 6, (col - strlen(error_message))/2, "%s", error_message);
        attroff(A_BOLD);
    }

    mvprintw(row - 2, 2, "F10: Quit");
}

void page_room_list_handle_input(ClientContext* ctx, int input) {
    // Request room list when first entering page
    if (!list_requested && ctx->connected) {
        client_send_message(ctx, "LIST_ROOMS", "");
        list_requested = true;
        strcpy(ctx->status_message, "Loading rooms...");
    }

    if (mode == 0) {
        // Room list mode
        if (input == KEY_UP) {
            if (ctx->room_count > 0) {
                selected_room = (selected_room - 1 + ctx->room_count) % ctx->room_count;
            }
        } else if (input == KEY_DOWN) {
            if (ctx->room_count > 0) {
                selected_room = (selected_room + 1) % ctx->room_count;
            }
        } else if (input == 27) { // ESC
            error_message[0] = '\0';
            room_id_input[0] = '\0';
            list_requested = false;
            selected_room = 0;
            ctx->status_message[0] = '\0';
            ctx->current_state = PAGE_DASHBOARD;
        } else if (input == 'r' || input == 'R') {
            // Refresh room list
            if (ctx->connected) {
                client_send_message(ctx, "LIST_ROOMS", "");
                strcpy(ctx->status_message, "Refreshing...");
            }
        } else if (input == 'm' || input == 'M') {
            // Switch to manual input mode
            mode = 1;
            room_id_input[0] = '\0';
            error_message[0] = '\0';
        } else if (input == 'h' || input == 'H') {
            // Rejoin as host
            if (ctx->room_count > 0 && selected_room < ctx->room_count) {
                RoomInfo* r = &ctx->rooms[selected_room];
                if (r->is_my_room) {
                    if (ctx->connected) {
                        client_send_message(ctx, "REJOIN_HOST", ctx->username);
                        strcpy(ctx->status_message, "Rejoining as host...");
                        error_message[0] = '\0';
                        list_requested = false;
                    }
                } else {
                    strcpy(error_message, "You are not the host of this room!");
                }
            }
        } else if (input == '\n' || input == KEY_ENTER) {
            // Join selected room
            if (ctx->room_count > 0 && selected_room < ctx->room_count) {
                RoomInfo* r = &ctx->rooms[selected_room];
                
                if (r->is_my_room) {
                    // Rejoin as host
                    if (ctx->connected) {
                        client_send_message(ctx, "REJOIN_HOST", ctx->username);
                        strcpy(ctx->status_message, "Rejoining as host...");
                        error_message[0] = '\0';
                        list_requested = false;
                    }
                } else {
                    // Join as participant
                    if (!ctx->connected) {
                        strcpy(error_message, "Not connected to server!");
                        return;
                    }
                    
                    char data[100];
                    sprintf(data, "%d,%s", r->id, ctx->username);
                    client_send_message(ctx, "JOIN_ROOM", data);
                    ctx->current_room_id = r->id;
                    sprintf(ctx->status_message, "Joining room %d...", r->id);
                    error_message[0] = '\0';
                    list_requested = false;
                }
            } else {
                strcpy(error_message, "No room selected!");
            }
        }
    } else {
        // Manual input mode
        if (input == 27) { // ESC
            mode = 0;
            room_id_input[0] = '\0';
            error_message[0] = '\0';
        } else if (input == '\n' || input == KEY_ENTER) {
            if (strlen(room_id_input) == 0) {
                strcpy(error_message, "Please enter a Room ID!");
                return;
            }
            
            int room_id = atoi(room_id_input);
            if (room_id <= 0) {
                strcpy(error_message, "Invalid Room ID!");
                return;
            }
            
            if (!ctx->connected) {
                strcpy(error_message, "Not connected to server!");
                return;
            }
            
            char data[100];
            sprintf(data, "%d,%s", room_id, ctx->username);
            client_send_message(ctx, "JOIN_ROOM", data);
            ctx->current_room_id = room_id;
            sprintf(ctx->status_message, "Joining room %d...", room_id);
            error_message[0] = '\0';
            room_id_input[0] = '\0';
            mode = 0;
            list_requested = false;
        } else if (input == KEY_BACKSPACE || input == 127) {
            if (strlen(room_id_input) > 0) {
                room_id_input[strlen(room_id_input) - 1] = '\0';
            }
        } else if (input >= '0' && input <= '9') {
            if (strlen(room_id_input) < 10) {
                size_t len = strlen(room_id_input);
                room_id_input[len] = input;
                room_id_input[len + 1] = '\0';
            }
        }
    }
}
