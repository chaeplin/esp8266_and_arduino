import processing.serial.*;

final int baudRate = 5600;

Serial port;
int serialPortIndex = -1;
final int bufferLength = 512;
FloatList x;
FloatList y;
FloatList z;

String rangeStr;
int range = -1;
int hp = 0;

void setup()
{
  size(900, 600, P2D);
  background(0);
  noFill();
 
  String[] serialList = Serial.list();
  text("Select serial port:", 10, 20);
  for(int i = 0; i < serialList.length; i++) {
    text((i+1) + ": " + serialList[i], 10, 40 + 12 * i); 
  }
  
  x = new FloatList();
  y = new FloatList();
  z = new FloatList();
}

void draw()
{
  if (serialPortIndex >= 0)
  {
    background(0);
    int n = x.size();
    fill(255);
    if (n <= 0) {
      text("Waiting for data", 10, 20);
      return;
    }
    
    text(rangeStr + " (1-3)", 10, 20);
    text("High pass: " + hp + " (Q-T)", 10, 40);
    
    float scaleX = width / (float)(bufferLength - 1);
    noFill();
    
    float scale = pow(2, range + 1);
    
    // x
    stroke(255, 0, 0);
    beginShape();
    for (int i = 0; i < n; i++) {
      float v = map(x.get(i), -scale, scale, 0, height);
      vertex(i * scaleX, v);
    }
    endShape();
    
    // y
    stroke(0, 255, 0);
    beginShape();
    for (int i = 0; i < n; i++) {
      float v = map(y.get(i), -scale, scale, 0, height);
      vertex(i * scaleX, v);
    }
    endShape();
    
    // z
    stroke(0, 0, 255);
    beginShape();
    for (int i = 0; i < n; i++) {
      float v = map(z.get(i), -scale, scale, 0, height);
      vertex(i * scaleX, v);
    }
    endShape();
  }
}

void keyPressed() {
  if (keyCode >= 49 && keyCode <= 57)
  {
    int index = keyCode - 49;
    if (serialPortIndex < 0)
    {
      initSerial(index);
    } else {
      setRange(index);
    }
  } else if (key == 'q'){
    port.write('q');
    hp = 0;
  } else if (key == 'w'){
    port.write('w');
    hp = 1;
  } else if (key == 'e'){
    port.write('e');
    hp = 2;
  } else if (key == 'r'){
    port.write('r');
    hp = 3;
  } else if (key == 't'){
    port.write('t');
    hp = 4;
  }
}

void initSerial(int index)
{
  print("Initializing port ");
  println(index);
  String[] serialList = Serial.list();
  if (index < serialList.length) {
    try {
      port = new Serial(this, serialList[index], baudRate);
      port.bufferUntil('\n');
      println("Serial port initialized");
      serialPortIndex = index;
      delay(1000);
    } catch (Exception e) {
      println("Couldn't initialize serial");
    }
  }
}

void serialEvent(Serial port) {
  if (range < 0) {
    setRange(0);
    delay(500);
  }
  String inString = port.readStringUntil('\n');
//  println(inString);
  String arr[] = inString.split(" ");
  x.append(parseFloat(arr[0]));
  y.append(parseFloat(arr[1]));
  z.append(parseFloat(arr[2]));
  if (x.size() > bufferLength) {
    x.remove(0);
    y.remove(0);
    z.remove(0);
  }
 
}

void setRange(int newRange) {
  if (newRange <= 2) {
    if (newRange == range) return;
    x.clear();
    y.clear();
    z.clear();
    range = newRange;
    rangeStr = "Range: ";
    switch (range) {
      case 0:
        rangeStr += "2G";
        break;
      case 1:
        rangeStr += "4G";
        break;
      case 2:
        rangeStr += "8G";
        break;
    }
    port.write(range);
  }
}