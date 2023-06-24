#include <stdio.h>
#include <string.h>

#define miliseconds 0.001
#define width 5
#define height 5

typedef struct {
    const char name_p[128];
    char player_char;
    int score;
} player_t;

typedef struct gamedata {
    player_t* one_p;
    player_t* two_p;
    player_t* player_turn_p;

    int games_passed;

    char board[3][3];

    void (*clear_board) (struct gamedata*);
} gamedata_t;

bool main_menu(gamedata_t*);
player_t* game_loop(gamedata_t*);
bool end_screen(player_t*, gamedata_t*);

bool is_round_over (gamedata_t*);
// players
void reset_player (player_t*);
player_t* get_other_player (player_t*);

// drawing
void draw_input_box (bool is_last, char c);
void draw_intermediate_layer ();

void clear_brd (gamedata_t*);

// other crap
void put_error_in_buffer (char buffer[], char* err);

int main (int argc, char** argv) {

    // starting
    gamedata_t* data_p = (gamedata_t*)malloc(sizeof(gamedata_t));
    data_p->one_p = (player_t*)malloc(sizeof(player_t));
    data_p->two_p = (player_t*)malloc(sizeof(player_t));

    reset_player(data_p->one_p);
    reset_player(data_p->two_p);

    data_p->player_turn_p = data_p->one_p;
    data_p->games_passed = 0;
    // function ptrs =D
    data_p->clear_board = clear_brd;

// managing controllflow with tags (bad practise in bigger projects, but fine here)
start:
    if (!main_menu(data_p)) {
        goto end;
    }

    end_screen(game_loop(data_p), data_p);

    goto start;

end:
    system("clear");
    printf("Thanks for playing!\n");
    player_t* winner = (data_p->one_p->score > data_p->two_p->score ) ? data_p->one_p : data_p->two_p;
    printf("Winner: %s\n", winner->name_p);
    printf("Score: \n");
    printf("    - %s: %d points\n", data_p->one_p->name_p, data_p->one_p->score);
    printf("    - %s: %d points\n", data_p->two_p->name_p, data_p->two_p->score);
    printf("Total rounds played: %d\n", data_p->games_passed);
    printf("\n");
    printf(" - A game developed by Joost van Haag - \n");

    free(data_p->one_p);
    free(data_p->two_p);
    free(data_p);
    return 0;
}

// yaay memset wrapper (don't touch the pointer 0.0)
void reset_player (player_t* ptr) {
    memset((void*)ptr->name_p, 0, sizeof(char) * 128);
    ptr->player_char = ' ';
    ptr->score = 0;   
}

bool main_menu (gamedata_t* data) {
    system("clear");

    // reset board (function pointer just cuz I felt cute)
    data->clear_board(data);

    if (data->games_passed == 0) {

        printf("Welcome to tick tack toe!\n");
        char name_buffer[128];

        // name
        printf("Name of player 1: ");
        scanf("%s", name_buffer);
        memcpy((void*)data->one_p->name_p, name_buffer, sizeof(char) * 128);


        // name
        printf("Name of player 2: ");
        scanf("%s", name_buffer);
        memcpy((void*)data->two_p->name_p, name_buffer, sizeof(char) * 128);

        // TODO: custom chars
        data->one_p->player_char = 'X';
        data->two_p->player_char = 'O';
        return true;
    }

    printf("Want to play again?\n");
    char buffer[128];

    printf("[y/n] ");
    scanf("%s", buffer);
    // lets hope they don't overflow the buffer :clown:
    return (strcasecmp(buffer, "yes") == 0 || strcasecmp(buffer, "y") == 0);
}

// main game logic
player_t* game_loop (gamedata_t* data_p) {

    system("clear");
    const int min_width = 5;
    const int min_height = 5;

    // TODO: this does not work for resizing yet
   
    char error_buffer[56];
    memset(error_buffer, 0, sizeof(error_buffer));

    while (true) {
        system("clear");
        printf("%s: %d\n", data_p->one_p->name_p, data_p->one_p->score);
        printf("%s: %d\n", data_p->two_p->name_p, data_p->two_p->score);
        putchar('\n');

        for (int i = 0; i < 3; i++) {
            bool is_last = (i == 2);
            for (int j = 0; j < 3; j++) {
                draw_input_box((j == 2), data_p->board[i][j]);
            }
            putchar('\n');
            if (!is_last) draw_intermediate_layer();
            putchar('\n');
        }

       
        while (true) {
    
            player_t* current = data_p->player_turn_p;
            player_t* opposing;
            // weird impl but sure
            if (current == data_p->one_p) {
                opposing = data_p->two_p;
            } else {
                opposing = data_p->one_p;
            }
            
            if (current == NULL) {
                // end the game cuz we have been scammed lol
                return NULL;
            }

            if (error_buffer[0] != '\0') {
                printf("[ERROR]: %s\n", error_buffer);
            } else {
                putchar('\n');
            }

            printf("%s's turn\n", current->name_p);
            printf("Give the coords to the tile you want to set!\n");
            int x;
            int y;
            printf("Give X coord 0 <-> 2: ");
            scanf("%d", &x);
            if (x < 0 || x > 2) {
                put_error_in_buffer(error_buffer, "that coord (X) was out of bounds!");
                break;
            }
            printf("Give Y coord 0 <-> 2: ");
            scanf("%d", &y);
            if (y < 0 || y > 2) {
                put_error_in_buffer(error_buffer, "that coord (Y) was out of bounds!");
                break;
            }

            memset(error_buffer, 0, sizeof(error_buffer));
            
            // handle turn
            char ch = data_p->board[y][x];
            if (ch != ' ') {
                put_error_in_buffer(error_buffer, "That place is already set!");
                break;
            }
            data_p->board[y][x] = current->player_char;

            // check for winner (TODO: look into checking for a winner by using bit masking)
            char cur_char = current->player_char;
            bool did_win = false;
            bool center = y == 1 && x == 1;
            bool corner = (y == 0 || y == 2) && (x == 0 || x == 2);
            bool sides = (!center && !corner);

            int good_hor = 0;
            int good_vert = 0;
            int good_side_1 = 0;
            int good_side_2 = 0;
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 3; j++) {
                    if (data_p->board[i][j] == cur_char) {
                        good_hor++;
                    }
                    if (data_p->board[j][i] == cur_char) {
                        good_vert++;
                    }

                    if (data_p->board[i][j] == cur_char && (i == j)) {
                        good_side_1++;
                    }
                    if (data_p->board[i][j] == cur_char && (i == j || (j == 2 && i == 0 || j == 0 && i == 2))) {
                        good_side_2++;
                    }

                    if (good_vert == 3 || good_hor == 3 || good_side_2 == 3 || good_side_1 == 3) {
                        return current;
                    } 
                }
                good_hor = good_vert = 0;
            }

            data_p->player_turn_p = opposing;

            break;
            // you never know ;-;
            sleep(10 * miliseconds);
        }
        // if this is reached + the check is passed that means no one won =/
        if (is_round_over(data_p)) {
            return NULL; 
        }
    }
    return NULL;
}

void draw_input_box (bool is_last, char c) {
    for (int i = 0; i < 3; i++) {
        if (i == 1) {
            putchar(c);
        } else {
            putchar(' ');
        }
    }
    if (!is_last) putchar('|');
}

void draw_intermediate_layer () {
    for (int i = 0; i < 11; ++i) {
        putchar('=');
    }
}

void clear_brd (gamedata_t* data) {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            data->board[i][j] = ' ';
        }
    }
}

bool is_round_over (gamedata_t* data) {
    for (int x = 0; x < 3; x++) {
        for (int y = 0; y < 3; y++) {
            if (data->board[x][y] == ' ') {
                return false;
            }
        }
    }
    return true;
}

// use 56 char long buffers
void put_error_in_buffer (char buffer[], char* err) {
    if (sizeof(err) > sizeof(char) * 56) {
        printf("buffer overflow =[\n");
        // nothing
    }
    memcpy(buffer, err, sizeof(char) * 56);
}

bool end_screen (player_t* winner, gamedata_t* data) {
    system("clear");
    if (winner == NULL) {
        printf("No winner =(\n");
    } else {
        printf("%s was the winner! =D\n", winner->name_p);
        putchar('\n');
        printf("Good job\n");
        winner->score++;
        data->player_turn_p = data->one_p;
        if (winner == data->one_p) {
            data->player_turn_p = data->two_p;
        }
    }
    data->games_passed++;

    
    return true;
}
