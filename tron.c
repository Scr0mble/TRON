// #define _POSIX_C_SOURCE 200112L /* Or higher */
#define _GNU_SOURCE

#include <curses.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "scheduler.h"
#include "util.h"

#include <pthread.h>
#include <semaphore.h>

// Defines used to track the player direction
#define DIR_NORTH 0
#define DIR_EAST 1
#define DIR_SOUTH 2
#define DIR_WEST 3

// Game parameters
#define INIT_player_LENGTH 4
#define player_HORIZONTAL_INTERVAL 100
#define player_VERTICAL_INTERVAL 150
#define DRAW_BOARD_INTERVAL 33
#define READ_INPUT_INTERVAL 150
#define BOARD_WIDTH 100
#define BOARD_HEIGHT 40

// Locks for concurrency control
pthread_mutex_t board_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t input_lock = PTHREAD_MUTEX_INITIALIZER;

/**
 * In-memory representation of the game board
 * Zero represents an empty cell
 * Positive numbers represent player cells (which count up at each time step until they reach
 * player_length) Negative numbers represent apple cells (which count up at each time step)
 */
int board[BOARD_HEIGHT][BOARD_WIDTH];

// player 1 parameters
int player_dir = DIR_NORTH;
int updated_player_dir = DIR_NORTH;

// player 2 parameters
int player_dir_2 = DIR_SOUTH;
int updated_player_dir_2 = DIR_SOUTH;


// Is the game running?
bool running = true;

/**
 * Convert a board row number to a screen position
 * \param   row   The board row number to convert
 * \return        A corresponding row number for the ncurses screen
 */
int screen_row(int row) {
  return 2 + row;
}

/**
 * Convert a board column number to a screen position
 * \param   col   The board column number to convert
 * \return        A corresponding column number for the ncurses screen
 */
int screen_col(int col) {
  return 2 + col;
}

/**
 * Initialize the board display by printing the title and edges
 */
void init_display() {
  // Print Title Line
  move(screen_row(-2), screen_col(BOARD_WIDTH / 2 - 5));
  addch(ACS_DIAMOND);
  addch(ACS_DIAMOND);
  printw(" Tron! ");
  addch(ACS_DIAMOND);
  addch(ACS_DIAMOND);

  // Print corners
  mvaddch(screen_row(-1), screen_col(-1), ACS_ULCORNER);
  mvaddch(screen_row(-1), screen_col(BOARD_WIDTH), ACS_URCORNER);
  mvaddch(screen_row(BOARD_HEIGHT), screen_col(-1), ACS_LLCORNER);
  mvaddch(screen_row(BOARD_HEIGHT), screen_col(BOARD_WIDTH), ACS_LRCORNER);

  // Print top and bottom edges
  for (int col = 0; col < BOARD_WIDTH; col++) {
    mvaddch(screen_row(-1), screen_col(col), ACS_HLINE);
    mvaddch(screen_row(BOARD_HEIGHT), screen_col(col), ACS_HLINE);
  }

  // Print left and right edges
  for (int row = 0; row < BOARD_HEIGHT; row++) {
    mvaddch(screen_row(row), screen_col(-1), ACS_VLINE);
    mvaddch(screen_row(row), screen_col(BOARD_WIDTH), ACS_VLINE);
  }

  // Refresh the display
  refresh();
}

/**
 * Show a game startup message and wait for a key press.
 */
void start_game() {
  int row =  (BOARD_HEIGHT / 2);
  int col = (BOARD_WIDTH / 2);
  mvprintw(screen_row(row) - 5, screen_col(col) - 6, "            ");
  mvprintw(screen_row(row) - 4, screen_col(col) - 9, " Welcome to Tron! ");
  mvprintw(screen_row(row) - 3, screen_col(col) - 6, "            ");
  mvprintw(screen_row(row) -2, screen_col(col) - 11, " The rules are simple: ");
  mvprintw(screen_row(row) - 1, screen_col(col) - 9, " 1. Walls kill you. ");
  mvprintw(screen_row(row), screen_col(col) - 12, " 2. Bike trails kill you. ");
  mvprintw(screen_row(row) + 1, screen_col(col) - 6, " 3. Survive! ");
  mvprintw(screen_row(row) + 2, screen_col(col) - 6, "            ");
  mvprintw(screen_row(row) + 3, screen_col(col) - 11, " Player 1: Arrow Keys ");
  mvprintw(screen_row(row) + 4, screen_col(col) - 10, " Player 2: WASD Keys ");
  mvprintw(screen_row(row) + 5, screen_col(col) - 6, "            ");
  refresh();

  // wait for user input
  mvprintw(screen_row(row) + 6, screen_col(col) - 12,
           " Press any key to begin. ");
  int key;
  while((key = getch())== ERR){
    ;
  }
  // ungetch otherwise it breaks
  ungetch(0);

  // countdown timer until game start
  for(int i = 3; i >= 0; i--){
    mvprintw(screen_row(row) + 6, screen_col(BOARD_WIDTH / 2) - 12,
           "     Starting in %d.     ", i);
    refresh();
    sleep(1);
  }
  mvprintw(screen_row(row) + 6, screen_col(col) - 12, "                        ");
  mvprintw(screen_row(row) + 6, screen_col(col) - 4,
           " Begin! ");

  refresh();
  sleep(2);
}

/**
 * Show a game over message and wait for a key press.
 */
void end_game(int player_num) {
  mvprintw(screen_row(BOARD_HEIGHT / 2) - 1, screen_col(BOARD_WIDTH / 2) - 6, "            ");
  mvprintw(screen_row(BOARD_HEIGHT / 2), screen_col(BOARD_WIDTH / 2) - 6, " Game Over! ");

  if(player_num == 0){
    mvprintw(screen_row(BOARD_HEIGHT / 2) + 1, screen_col(BOARD_WIDTH / 2) - 3, " Draw! ");
  } else {
    mvprintw(screen_row(BOARD_HEIGHT / 2) + 1, screen_col(BOARD_WIDTH / 2) - 8, " Player %d wins! ", player_num);
  }

  mvprintw(screen_row(BOARD_HEIGHT / 2) + 2, screen_col(BOARD_WIDTH / 2) - 11,
           "Press any key to exit.");
  refresh();
  timeout(-1);

  // sleep so we don't accidentally exit right away
  sleep(1);

  // wait for user input
  int key = getch();
  while(key == ERR){
    ;
  }
}

/**
 * Run in a task to draw the current state of the game board.
 */
void* draw_board(void* arg) {

  // define color pairs for the bikes and trails
  start_color();
  init_pair(1, COLOR_WHITE, COLOR_YELLOW); // color pair for player 1 trail
  init_pair(2, COLOR_BLACK, COLOR_CYAN); // color pair for player 2 trail
  init_pair(3, COLOR_BLACK, COLOR_WHITE); // color pair for player bikes

  while (running) {
    refresh();
    // Loop over cells of the game board
    pthread_mutex_lock(&board_lock);
    for (int r = 0; r < BOARD_HEIGHT; r++) {
      for (int c = 0; c < BOARD_WIDTH; c++) {
        if (board[r][c] == 0) {  // Draw blank spaces
          mvaddch(screen_row(r), screen_col(c), ' ');
        } else if (board[r][c] == 1 || board[r][c] == 3){ // Draw player bikes
          mvaddch(screen_row(r), screen_col(c), ' '| COLOR_PAIR(3));
        } else if (board[r][c] == 2) {  // Draw player 1 trail
          mvaddch(screen_row(r), screen_col(c), ' '| COLOR_PAIR(1));
        }else if (board[r][c] == 4) {  // Draw player 2 trail
          mvaddch(screen_row(r), screen_col(c), ' '| COLOR_PAIR(2));
        }
      }
    }
    pthread_mutex_unlock(&board_lock);

    // Draw the score
    // mvprintw(screen_row(-2), screen_col(BOARD_WIDTH - 9), "Score %03d\r",
    //          player_length - INIT_player_LENGTH);

    // Refresh the display
    //refresh();

    // Sleep for a while before drawing the board again
    sleep_ms(DRAW_BOARD_INTERVAL);

  }
  return NULL;
}

/**
 * Run in a task to process user input.
 */
void* read_input(void *arg) {

  int key;

  while (running) {

    // Make sure the input was read correctly
    if ((key = getch()) == ERR) {
      // ungetch(0);
      continue;
      // end_game();
      // sleep(3);
      // running = false;
      // end_game();
    }

    // Handle the key press
    if (key == KEY_UP && player_dir != DIR_SOUTH) {
      updated_player_dir = DIR_NORTH; // move player 1 up
    } else if (key == KEY_RIGHT && player_dir != DIR_WEST) {
      updated_player_dir = DIR_EAST; // move player 1 right
    } else if (key == KEY_DOWN && player_dir != DIR_NORTH) {
      updated_player_dir = DIR_SOUTH; // move player 1 down
    } else if (key == KEY_LEFT && player_dir != DIR_EAST) {
      updated_player_dir = DIR_WEST; // move player 1 left
    } else if (key == 'w' && player_dir_2 != DIR_SOUTH) {
      updated_player_dir_2 = DIR_NORTH; // move player 2 up
    } else if (key == 'd' && player_dir_2 != DIR_WEST) {
      updated_player_dir_2 = DIR_EAST; // move player 2 right 
    } else if (key == 's' && player_dir_2 != DIR_NORTH) {
      updated_player_dir_2 = DIR_SOUTH; // move player 2 down 
    } else if (key == 'a' && player_dir_2 != DIR_EAST) {
      updated_player_dir_2 = DIR_WEST; // move player 2 left
    } //else if (key == 'q') {
    //   running = false;
    //   end_game(0); // end the game early
    // }
  }
  return NULL;
}

/**
 * Run in a task to move the player around on the board
 */
void* update_player(void* arg) {

  while (running) {
  
    int player_num = *(int*)arg;
    int current_player_dir;

    // Update the direction of the player
    if(player_num == 1){
      current_player_dir = updated_player_dir;
      player_dir = updated_player_dir;
    }
    else{
      player_dir_2 = updated_player_dir_2;
      current_player_dir = updated_player_dir_2;
    }

    // player_dir = updated_player_dir;
    // player_dir_2 = updated_player_dir_2;

    int player_row;
    int player_col;

    // int player_row_2;
    // int player_col_2;

    pthread_mutex_lock(&board_lock);
    // "Age" each existing segment of the player
    for (int r = 0; r < BOARD_HEIGHT; r++) {
      for (int c = 0; c < BOARD_WIDTH; c++) {
        if (board[r][c] == player_num *2 -1) {  // Found the bike of the player. Save position
          player_row = r;
          player_col = c;
          board[r][c]++;
        }
      }
    }
    pthread_mutex_unlock(&board_lock);

    // Move the player into a new space
    if (current_player_dir == DIR_NORTH) {
      player_row--;
    } else if (current_player_dir == DIR_SOUTH) {
      player_row++;
    } else if (current_player_dir == DIR_EAST) {
      player_col++;
    } else if (current_player_dir == DIR_WEST) {
      player_col--;
    }

    // Check for edge collisions
    pthread_mutex_lock(&board_lock);
    if (player_row < 0 || player_row >= BOARD_HEIGHT || player_col < 0 || player_col >= BOARD_WIDTH) {
      running = false;
      end_game(2 / player_num); // current thread lost, so we pass the other player num
    // Check for head-to-head collisions
    } else if (board[player_row][player_col] != 0 && board[player_row][player_col] == 3 / player_num){
      running = false;
      end_game(0);
    // Check for player collisions
    } else if (board[player_row][player_col] != 0) {
      running = false;
      end_game(2 / player_num); 
    }
    // if no collisions, update the new position of the bike
    if (running){
      board[player_row][player_col] = (player_num * 2) - 1;
    }
    pthread_mutex_unlock(&board_lock);

    // Update the player movement speed to deal with rectangular cursors
    if (current_player_dir == DIR_NORTH || current_player_dir == DIR_SOUTH) {
      sleep_ms(player_VERTICAL_INTERVAL);
    } else {
      sleep_ms(player_HORIZONTAL_INTERVAL);
    }

  }
  return NULL;
}

// Entry point: Set up the game, create jobs, then run the scheduler
int main(void) {

  // Initialize the ncurses window
  WINDOW* mainwin = initscr();
  if (mainwin == NULL) {
    fprintf(stderr, "Error initializing ncurses.\n");
    exit(2);
  }

  // Seed random number generator with the time in milliseconds
  srand(time_ms());

  noecho();                // Don't print keys when pressed
  keypad(mainwin, true);   // Support arrow keys
  nodelay(mainwin, true);  // Non-blocking keyboard access

  // Initialize the game display
  init_display();

  // Zero out the board contents
  memset(board, 0, BOARD_WIDTH * BOARD_HEIGHT * sizeof(int));

  // Put the player at the middle of the board
  board[BOARD_HEIGHT - 2][BOARD_WIDTH / 2] = 1;
  board[2][BOARD_WIDTH / 2] = 3;

  // Threads for each of the game tasks
  pthread_t update_player_thread;
  pthread_t update_player_thread_2;
  pthread_t draw_board_thread;
  pthread_t read_input_thread;

  // display a starting screen
  start_game();
  wrefresh(mainwin);

  int player_number_1 = 1;
  int player_number_2 = 2;

  // create the four game threads
  pthread_create(&update_player_thread, NULL, update_player, (void*) &player_number_1);
  pthread_create(&update_player_thread_2, NULL, update_player, (void*) &player_number_2);
  pthread_create(&draw_board_thread, NULL, draw_board, NULL);
  pthread_create(&read_input_thread, NULL, read_input, NULL);

  // wait for all threads to finish
  pthread_join(update_player_thread, NULL);
  pthread_join(update_player_thread_2, NULL);
  pthread_join(draw_board_thread, NULL);
  pthread_join(read_input_thread, NULL);



  // Display the end of game message and wait for user input **now handled in update_player**
  // end_game();

  // Clean up window
  delwin(mainwin);
  endwin();

  return 0;
}
