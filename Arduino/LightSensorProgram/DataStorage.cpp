#include "DataStorage.h"
/* The storage class deals with the microSD card, and file management within the card. */
/* Initialize the class with chip select pin and file name */
Storage::Storage(int CS, String filename)
: CS_PIN(CS), log_file_name(filename + LOG_FILE_EXT) {}

/* Attempt to initialize the SD card reader. If unsuccessful, set error flag */
void Storage::init() {
  // SD is not opened if NO_SAVE is true
  if (!NO_SAVE && !SD.begin( CS_PIN) ) {
    // unable to open SD card, set error flag
    ERR = true;
    return;
  }
  
//      // open the metadata file and read data counter if it exists
//      String metadata_file_name = log_file_name + METADATA_SUFFIX + LOG_FILE_EXT;
//      if (SD.exists(metadata_file_name)) {
//        File metadata_file = SD.open(metadata_file_name, FILE_READ);
//        
//        if (!metadata_file) {
//          ERR = true;
//          return;
//        }
//        
//        String line = metadata_file.readStringUntil('\n');
//        // read the integer into data_counter
//        data_counter = line.toInt();
//        metadata_file.close();
//      }
}

/* Once the SD card reader is initialized, attempt to open the log file */
void Storage::open_file() {
  // attempt to open SD card for writing
  int i = 0;
  for (i = 0; i < 5; i++) {
    log_file = SD.open(log_file_name, FILE_WRITE);
    if (log_file) break;
  }

  if (i == 5) {
    ERR = true;
    return;
  }

  // file opened, check if new file
  if (log_file.size() == 0) {
    // brand new file, add headers
    String line = "DATE,TIME,MANUAL,INT_TIME,FRAME_AVG,AE,IS_SATURATED,X,Y,Z,";

    // add the wavelengths to the header
    for (int i = MIN_WAVELENGTH; i <= MAX_WAVELENGTH; i += WAVELENGTH_STEPSIZE) {
      line.concat(String(i));
      line.concat(",");
    }
    // finally write the header string to file
    write_line(&line);
  }
}

/* Close the SD file */
void Storage::close_file() {
  log_file.close();
}

void Storage::delete_file() {
  SD.remove(String(LOG_FILENAME) + String(LOG_FILE_EXT));
  SD.remove(String(LOG_FILENAME) + String(METADATA_SUFFIX) + String(LOG_FILE_EXT));
}

/* Open the file, Write a line, then close the file. */
void Storage::write_line(String *line) {
  if (NO_SAVE) return; // skip if no save flag is set
  open_file();
  log_file.println(*line);
  
  close_file();

//      // open the metadata file and write counter to it
//      String metadata_file_name = log_file_name + METADATA_SUFFIX + LOG_FILE_EXT;
//      if (SD.exists(metadata_file_name)) {
//        // delete the file before updating
//        SD.remove(metadata_file_name);
//      }
//      // create it brand new
//      File metadata_file = SD.open(metadata_file_name, FILE_WRITE);
//      
//      if (!metadata_file) {
//        ERR = true;
//        return;
//      }
//      
//      metadata_file.println(data_counter);
//      metadata_file.close();
}

/* Read a line given line number from the SD file */
String Storage::read_line(unsigned int line) {
  String line_to_ret = "";

  log_file.seek(0);
  char cr;

  for (unsigned int i = 0; i < (line - 1); i++) {
    cr = log_file.read();
    if (cr == '\n') {
      i++;
    }
  }

  for (unsigned int i = 0; i < 5000; i++) {
    cr = log_file.read();
    line_to_ret += cr;
    if (cr == '\n') {
      break;
    }
  }
  return line_to_ret;
}

/* If SD is errored, return true, else return false. */
bool Storage::is_errored() {
  return ERR;
}