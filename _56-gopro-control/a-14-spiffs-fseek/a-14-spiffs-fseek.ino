
int CHUNKED_FILE_SIZE = 292000;   // 146KB
//int FILE_SIZE         = 1813461;  // 1.8M
int FILE_SIZE         = 2827855;

void setup() {
  Serial.begin(115200);
  Serial.println("");
  Serial.flush();

  int x   = 0;
  int sum = 0;
  
  while (1) {
    int get_size = CHUNKED_FILE_SIZE;
    int position = (CHUNKED_FILE_SIZE * x);

    if (x > 20 || position >= FILE_SIZE) {
      break;
    }

    if ( ( position + get_size) > FILE_SIZE ) {
      get_size = FILE_SIZE - position;
    }
    sum = sum + get_size;
    Serial.printf("chunk no :\t%d\tposition :\t%d\tget_size :\t%d\tlast : \t%d\n", x, position, get_size, (position + get_size -1));
    x++;
  }
  Serial.printf("sum : %d\n", sum);
  Serial.flush();

}

void loop() {
}
