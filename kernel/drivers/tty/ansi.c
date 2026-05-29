#include <kernel/tty.h>
#include <kernel/ansi.h>
#include <stdio.h>

int handle_ansi(tty_t* tty, uint8_t c){
    switch(tty->ansi_state){
        case ANSI_STATE_NORMAL:
            if(c == 0x1b){
                tty->ansi_state = ANSI_STATE_ESCAPE;
                return 1;
            } else {
                return 0;
            }
        case ANSI_STATE_ESCAPE:
            if(c == '['){
                tty->ansi_state = ANSI_STATE_CSI;
                tty->ansi_param_count = 0;
                tty->ansi_private = 0;
                for(int i = 0; i < 8; i++) tty->ansi_params[i] = 0;
                return 1;
            } else {
                // Invalid sequence, return to normal state
                tty->ansi_state = ANSI_STATE_NORMAL;
                return 0;
            }
        
        case ANSI_STATE_CSI:
            if(c == '?'){
                tty->ansi_private = 1;
                return 1;
            }
            if(c >= '0' && c <= '9'){
                if(tty->ansi_param_count == 0) tty->ansi_param_count = 1; // Start first param
                tty->ansi_params[tty->ansi_param_count - 1] *= 10;
                tty->ansi_params[tty->ansi_param_count - 1] += (c - '0');
                return 1;
            } else if(c == ';'){
                if(tty->ansi_param_count < 8) tty->ansi_param_count++;
                return 1;
            } else {
                tty->ansi_state = ANSI_STATE_NORMAL;
                switch(c){
                    case 'H': // Cursor position
                        if(tty->ansi_param_count >= 2){
                            int row = tty->ansi_params[0] - 1;
                            int col = tty->ansi_params[1] - 1;
                            tty_move_cursor(tty, col, row);
                        }else{
                            tty_move_cursor(tty, 0, 0);
                        }
                        break;
                    
                    case 'A': // Cursor up
                        if(tty->ansi_param_count >= 1){
                            int n = tty->ansi_params[0];
                            tty_move_cursor(tty, tty->cursor_x, tty->cursor_y - n);
                        } else {
                            tty_move_cursor(tty, tty->cursor_x, tty->cursor_y - 1);
                        }
                        break;
                    case 'B': // Cursor down
                        if(tty->ansi_param_count >= 1){
                            int n = tty->ansi_params[0];
                            tty_move_cursor(tty, tty->cursor_x, tty->cursor_y + n);
                        } else {
                            tty_move_cursor(tty, tty->cursor_x, tty->cursor_y + 1);
                        }
                        break;
                    case 'C': // Cursor forward
                        if(tty->ansi_param_count >= 1){
                            int n = tty->ansi_params[0];
                            tty_move_cursor(tty, tty->cursor_x + n, tty->cursor_y);
                        } else {
                            tty_move_cursor(tty, tty->cursor_x + 1, tty->cursor_y);
                        }
                        break;
                    case 'D': // Cursor backward
                        if(tty->ansi_param_count >= 1){
                            int n = tty->ansi_params[0];
                            tty_move_cursor(tty, tty->cursor_x - n, tty->cursor_y);
                        } else {
                            tty_move_cursor(tty, tty->cursor_x - 1, tty->cursor_y);
                        }
                        break;
                    case 'l':   // Hide cursor
                        if(tty->ansi_private && tty->ansi_params[0] == 25){
                            tty_set_cursor_visibility(tty,0);
                        }
                        break;
                    case 'h':   // Show cursor
                        if(tty->ansi_private && tty->ansi_params[0] == 25){
                            tty_set_cursor_visibility(tty,1);
                        }
                        break;        
                    case 'J':
                        if(tty->ansi_params[0] == 2){
                            tty_clear_screen(tty);
                        }
                        break;
                    case 'K':
                        if(tty->ansi_param_count >= 1){
                            if(tty->ansi_params[0] == 0){
                                tty_clear_line_from_cursor(tty);
                            }else if(tty->ansi_params[0] == 2){
                                tty_clear_line(tty);
                            }
                        }
                        else{
                            tty_clear_line_from_cursor(tty);
                        }
                        break;
                }
                return 1;
            }
    }
    return 0;
}