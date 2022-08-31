#ifndef DATASTORAGE_H
#define DATASTORAGE_H
#include <Arduino.h>
#include <SD.h>
#include "Constants.h"



/* The storage class deals with the microSD card, and file management within the card. */


class Storage {
  private:
    String log_file_name;                           // file name and directory to save the CSV data log to
    File log_file;                                  // the file variable that holds the log file
    int CS_PIN;                                     // the SPI chip select pin
    bool ERR;                                       // was there an error with the Storage class?

  public:
    /* Initialize the class with chip select pin and file name */  
    Storage(int CS, String filename);

    /* Attempt to initialize the SD card reader. If unsuccessful, set error flag */
    void init();

    /* Once the SD card reader is initialized, attempt to open the log file */
    void open_file();
  
    /* Close the SD file */
    void close_file();

    bool delete_file();
  
    /* Open the file, Write a line, then close the file. */
    void write_line(String *line);
  
    /* Read a line given line number from the SD file */
    String get_line(unsigned int line);

    /* Read a line and move onto the next. Consecutively calling this function returns all lines in file. */
    String read_line(unsigned int line, unsigned int buf_size);
  
    /* If SD is errored, return true, else return false. */
    bool is_errored();
};

#endif
