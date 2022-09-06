#include "DataStorage.h"
/* The storage class deals with the microSD card, and file management within the card. */
/* Initialize the class with chip select pin and file name */
Storage::Storage(int CS, String filename)
: CS_PIN(CS), log_file_name(filename + LOG_FILE_EXT), data_counter(0) {}

/* Attempt to initialize the SD card reader. If unsuccessful, set error flag */
void Storage::init() {
  // SD is not opened if NO_SAVE is true
  if (!NO_SAVE && !SD.begin( CS_PIN) ) {
    // unable to open SD card, set error flag
    ERR = true;
    return;
  }
  
  // open the metadata file and read data counter if it exists
  String metadata_file_name = log_file_name + METADATA_SUFFIX + LOG_FILE_EXT;
  if (SD.exists(metadata_file_name)) {
    File metadata_file = SD.open(metadata_file_name, FILE_READ);
    
    if (!metadata_file) {
      ERR = true;
      return;
    }
    
    String line = metadata_file.readStringUntil('\n');
    // read the integer into data_counter
    data_counter = line.toInt();
    metadata_file.close();
  }
}

unsigned int Storage::data_count() {
  return data_counter;
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

bool Storage::delete_file() {
  int res_a = SD.remove(String(LOG_FILENAME) + String(LOG_FILE_EXT));
  int res_b = SD.remove(String(LOG_FILENAME) + String(METADATA_SUFFIX) + String(LOG_FILE_EXT));
  if (res_a == res_b && res_a == 1) {
    return true;
  } else {
    return false;
  }
}

/* Open the file, Write a line, then close the file. */
void Storage::write_line(String *line) {
  if (NO_SAVE) return; // skip if no save flag is set
  open_file();
  log_file.println(*line);
  data_counter++;
  close_file();

  // open the metadata file and write counter to it
  String metadata_file_name = log_file_name + METADATA_SUFFIX + LOG_FILE_EXT;
  File metadata_file = SD.open(metadata_file_name, FILE_WRITE);
  // go to the beginning and overwrite
  metadata_file.seek(0);
  metadata_file.print(data_counter, DEC);
  // print 10 extra spaces to overwrite the the file
  for (int i = 0; i < 10; i++) {
    metadata_file.print(" ");
  }
  metadata_file.close();
}

/* Read a line given line number from the SD file */
String Storage::get_line(unsigned int line) {
  if (!log_file) {
    open_file();
  }
  
  String line_to_ret = "";

  log_file.seek(0);
  char c;

  for (unsigned int i = 0; i < (line - 1); i++) {
    c = log_file.read();
    if (c == '\n') {
      i++;
    }
  }

  for (unsigned int i = 0; i < 5000; i++) {
    c = log_file.read();
    line_to_ret += c;
    if (c == '\n') {
      break;
    }
  }

  close_file();
  
  return line_to_ret;
}

String Storage::read_line(unsigned int line, unsigned int buf_size) {
  String line_to_ret = "";
  char c;

  if (line == 0) log_file.seek(0);

  for (unsigned int i = 0; i < buf_size; i++) {
    c = log_file.read();
    if (c == '\n') {
      break;
    }
    line_to_ret += c;
  }

  return line_to_ret;
}

/* If SD is errored, return true, else return false. */
bool Storage::is_errored() {
  return ERR;
}
