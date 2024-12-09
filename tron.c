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

// Defines used to track the worm direction
#define DIR_NORTH 0
#define DIR_EAST 1
#define DIR_SOUTH 2
#define DIR_WEST 3

// Game parameters
#define INIT_WORM_LENGTH 4
#define WORM_HORIZONTAL_INTERVAL 200
#define WORM_VERTICAL_INTERVAL 300
#define DRAW_BOARD_INTERVAL 33
#define READ_INPUT_INTERVAL 150
#define BOARD_WIDTH 100
#define BOARD_HEIGHT 40

pthread_barrier_t barr;
pthread_mutex_t board_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t input_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

/**
 * In-memory representation of the game board
 * Zero represents an empty cell
 * Positive numbers represent worm cells (which count up at each time step until they reach
 * worm_length) Negative numbers represent apple cells (which count up at each time step)
 */
int board[BOARD_HEIGHT][BOARD_WIDTH];

// Worm parameters
int worm_dir = DIR_NORTH;
int worm_length = INIT_WORM_LENGTH;
int updated_worm_dir = DIR_NORTH;

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
  printw(" Worm! ");
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
 * Show a game over message and wait for a key press.
 */
void end_game() {
  mvprintw(screen_row(BOARD_HEIGHT / 2) - 1, screen_col(BOARD_WIDTH / 2) - 6, "            ");
  mvprintw(screen_row(BOARD_HEIGHT / 2), screen_col(BOARD_WIDTH / 2) - 6, " Game Over! ");
  mvprintw(screen_row(BOARD_HEIGHT / 2) + 1, screen_col(BOARD_WIDTH / 2) - 6, "            ");
  mvprintw(screen_row(BOARD_HEIGHT / 2) + 2, screen_col(BOARD_WIDTH / 2) - 11,
           "Press any key to exit.");
  refresh();
  timeout(-1);
  int key = getch();
  while(key == ERR){
    ;
  }
}

/**
 * Run in a task to draw the current state of the game board.
 */
void* draw_board(void* arg) {
  while (running) {
    // Loop over cells of the game board
    pthread_mutex_lock(&board_lock);
    for (int r = 0; r < BOARD_HEIGHT; r++) {
      for (int c = 0; c < BOARD_WIDTH; c++) {
        if (board[r][c] == 0) {  // Draw blank spaces
          mvaddch(screen_row(r), screen_col(c), ' ');
        } else if (board[r][c] == 1){
          mvaddch(screen_row(r), screen_col(c), 'H');
        } else if (board[r][c] > 0) {  // Draw worm
          mvaddch(screen_row(r), screen_col(c), 'O');
        }
      }
    }
    pthread_mutex_unlock(&board_lock);

    // Draw the score
    // mvprintw(screen_row(-2), screen_col(BOARD_WIDTH - 9), "Score %03d\r",
    //          worm_length - INIT_WORM_LENGTH);

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
  // pthread_mutex_lock(&dirk_lock);
  // while(created == 0)
  //   pthread_cond_wait(&cond, &dirk_lock);
  // pthread_mutex_unlock(&dirk_lock);
  int key;

  while (running) {

    // Read a character, potentially blocking this task until a key is pressed

    
    // Make sure the input was read correctly
    if ((key = getch()) == ERR) {
      // ungetch(0);
      continue;
      // end_game();
      // sleep(3);
      // running = false;
      // end_game();
    }


    // Make sure the input was read correctly
    // if (key == ERR) {
    //   continue;
    // }

    // Handle the key press
    if (key == KEY_UP && worm_dir != DIR_SOUTH) {
      updated_worm_dir = DIR_NORTH;
    } else if (key == KEY_RIGHT && worm_dir != DIR_WEST) {
      updated_worm_dir = DIR_EAST;
    } else if (key == KEY_DOWN && worm_dir != DIR_NORTH) {
      updated_worm_dir = DIR_SOUTH;
    } else if (key == KEY_LEFT && worm_dir != DIR_EAST) {
      updated_worm_dir = DIR_WEST;
    } else if (key == 'q') {
      running = false;
    }
  }
  return NULL;
}

/**
 * Run in a task to move the worm around on the board
 */
void* update_worm(void* arg) {
  // pthread_mutex_lock(&dirk_lock);
  // while(created == 0)
  //   pthread_cond_wait(&cond, &dirk_lock);
  // pthread_mutex_unlock(&dirk_lock);
  while (running) {
    // Update the direction of the worm
    worm_dir = updated_worm_dir;

    int worm_row;
    int worm_col;

    pthread_mutex_lock(&board_lock);
    // "Age" each existing segment of the worm
    for (int r = 0; r < BOARD_HEIGHT; r++) {
      for (int c = 0; c < BOARD_WIDTH; c++) {
        if (board[r][c] == 1) {  // Found the head of the worm. Save position
          worm_row = r;
          worm_col = c;
        }

        // Add 1 to the age of the worm segment
        if (board[r][c] > 0) {
          board[r][c]++;

        //    // Remove the worm segment if it is too old
        //    if (board[r][c] > worm_length) {
        //      board[r][c] = 0;
        //    }
        }
      }
    }
    pthread_mutex_unlock(&board_lock);

    // Move the worm into a new space
    if (worm_dir == DIR_NORTH) {
      worm_row--;
    } else if (worm_dir == DIR_SOUTH) {
      worm_row++;
    } else if (worm_dir == DIR_EAST) {
      worm_col++;
    } else if (worm_dir == DIR_WEST) {
      worm_col--;
    }

    // Check for edge collisions
    if (worm_row < 0 || worm_row >= BOARD_HEIGHT || worm_col < 0 || worm_col >= BOARD_WIDTH) {
      running = false;

      // Add a key to the input buffer so the read_input task can exit
      // ungetch(0);
    pthread_mutex_lock(&board_lock);
    } else if (board[worm_row][worm_col] > 0) {
      // Check for worm collisions
      running = false;

      // Add a key to the input buffer so the read_input task can exit
      // ungetch(0);
    } else if (board[worm_row][worm_col] < 0) {
      // Check for apple collisions
      // Worm gets longer
      worm_length++;
    }
    pthread_mutex_unlock(&board_lock);

    // Add the worm's new position
    pthread_mutex_lock(&board_lock);
    if (running) board[worm_row][worm_col] = 1;
    pthread_mutex_unlock(&board_lock);

    // Update the worm movement speed to deal with rectangular cursors
    if (worm_dir == DIR_NORTH || worm_dir == DIR_SOUTH) {
      sleep_ms(WORM_VERTICAL_INTERVAL);
    } else {
      sleep_ms(WORM_HORIZONTAL_INTERVAL);
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

  // Put the worm at the middle of the board
  board[BOARD_HEIGHT - 2][BOARD_WIDTH / 2] = 1;

  // Task handles for each of the game tasks
  // task_t update_worm_task;
  // task_t draw_board_task;
  // task_t read_input_task;

  // Threads for each of the game tasks
  pthread_t update_worm_thread;
  pthread_t draw_board_thread;
  pthread_t read_input_thread;

  // Initialize the scheduler library
  // scheduler_init();


  // Create tasks for each task in the game
  // task_create(&update_worm_task, update_worm);
  // task_create(&draw_board_task, draw_board);
  // task_create(&read_input_task, read_input);

  pthread_create(&update_worm_thread, NULL, update_worm, NULL);
  pthread_create(&draw_board_thread, NULL, draw_board, NULL);
  pthread_create(&read_input_thread, NULL, read_input, NULL);


  // // Wait for these tasks to exit
  // task_wait(update_worm_task);
  // task_wait(draw_board_task);
  // task_wait(read_input_task);
  
  pthread_join(update_worm_thread, NULL);
  pthread_join(draw_board_thread, NULL);
  pthread_join(read_input_thread, NULL);



  // Display the end of game message and wait for user input
  end_game();

  // Clean up window
  delwin(mainwin);
  endwin();

  return 0;
}
