/*
 * This code must be run on an Arduino MEGA, because the UNO does not have
 * enought SRAM memory to handle all the ROS publishers and messages. It 
 * causes errors immediately upon initializing the publishers and loses
 * connection with ROS.
 * 
 * IMU Connections
 * ===============
 * SCL -> A5 (Uno), 21 (MEGA)
 * SDA -> A4 (Uno), 20 (MEGA)
 * VDD -> 3-5V DC
 * GND -> Ground/Common
 * 
 * The calibration for the IMU resets everytime it turns off. In order to keep the 
 * calibration values, they are stored as ROS parameters and are read/saved in a 
 * separate node.
 */

// ROS Includes
#include <ros.h>
#include <sensors/ImuArray.h>
#include <sensors/ImuMag.h>
#include <sensors/ImuCalibStatus.h>
#include <std_msgs/Int16.h>

// IMU Includes
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>

// Encoder pins and variables
int ir_1 = 2;
int ir_2 = 3;
int ir_3 = 4;
int ir_4 = 5;

int counter1 = 0;
int counter2 = 0;

int oldenc_1 = 0;
int oldenc_2 = 0;
int oldenc_3 = 0;
int oldenc_4 = 0;

int state1;
int oldstate1;
int state2;
int oldstate2;

// ROS Objects
ros::NodeHandle nh;
// encoder messages (wheel1 is left, wheel2 is right)
std_msgs::Int16 rwheel_msg;
std_msgs::Int16 lwheel_msg;
ros::Publisher rwheel_pub("enc/wheel_right_ticks", &rwheel_msg);
ros::Publisher lwheel_pub("enc/wheel_left_ticks", &lwheel_msg);
// IMU
sensors::ImuArray imu_data;
ros::Publisher imu_pub("imu/data_array", &imu_data);
sensors::ImuMag imu_mag;
ros::Publisher mag_pub("imu/mag_array", &imu_mag);
sensors::ImuCalibStatus imu_calib_status;
ros::Publisher imu_status_pub("imu/status", &imu_calib_status);

// IMU Objects
Adafruit_BNO055 bno = Adafruit_BNO055(55);
imu::Quaternion quat;
imu::Vector<3> ang;
imu::Vector<3> lin;
imu::Vector<3> mag;

void setup() {
  // Debugging Pin
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);
  
  // Set pins for encoder IR sensors
  pinMode(ir_1, INPUT);
  pinMode(ir_2, INPUT);
  pinMode(ir_3, INPUT);
  pinMode(ir_4, INPUT);

  int ir1 = digitalRead(ir_1);
  int ir2 = digitalRead(ir_2);
  int ir3 = digitalRead(ir_3);
  int ir4 = digitalRead(ir_4);

  // State initializations
  if (ir1 == 0) {
    if (ir2 == 0) {
      oldstate1 = 0;
      }
    else {
      oldstate1 = 1;
    }
  }
    else {
      if (ir2 == 0) {
        oldstate1 = 2;
      }
    else {
      oldstate1 = 3;
    }
  }
  
  if (ir3 == 0) {
    if (ir4 == 0) { 
      oldstate2 = 0;
    }
    else {
      oldstate2 = 1;
    }
  }
  else {
    if (ir4 == 0) {
      oldstate2 = 2;
    }
    else {
      oldstate2 = 3;
    }
  }
  
  // Start ROS Node
  nh.initNode();
  nh.advertise(rwheel_pub);
  nh.advertise(lwheel_pub);
  nh.advertise(imu_pub);
  nh.advertise(mag_pub);
  nh.advertise(imu_status_pub);

  Serial.begin(57600);
  delay(100);

  // Wait for BNO055 to begin before continuing
  // This must be here or you won't get IMU data and ROS will freeze
  if (!bno.begin()) {
    //Serial.println("No BNO055 detected.");
    while(1);
  }

  // Get Default Calibration Parameters
  adafruit_bno055_offsets_t calibration_data;
  bno.getSensorOffsets(calibration_data);

  // Grab IMU calibration parameters, if parameter is not set, use default value
  int16_t accel_offset_x;
  int16_t accel_offset_y;
  int16_t accel_offset_z;
  int16_t gyro_offset_x;
  int16_t gyro_offset_y;
  int16_t gyro_offset_z;
  int16_t mag_offset_x;
  int16_t mag_offset_y;
  int16_t mag_offset_z;
  int16_t accel_radius;
  int16_t mag_radius;

  // Needs a delay before attempting to load parameters
  delay(1000);
  
  if (!nh.getParam("imu/calibration/accel_offset_x", &accel_offset_x)) {
    accel_offset_x = calibration_data.accel_offset_x;
  }
  if (!nh.getParam("imu/calibration/accel_offset_y", &accel_offset_y)) {
    accel_offset_y = calibration_data.accel_offset_y;
  }
  if (!nh.getParam("imu/calibration/accel_offset_z", &accel_offset_z)) {
    accel_offset_z = calibration_data.accel_offset_z;
  }
  if (!nh.getParam("imu/calibration/accel_radius", &accel_radius)) {
    accel_radius = calibration_data.accel_radius;
  }
  if (!nh.getParam("imu/calibration/gyro_offset_x", &gyro_offset_x)) {
    gyro_offset_x = calibration_data.gyro_offset_x;
  }
  if (!nh.getParam("imu/calibration/gyro_offset_y", &gyro_offset_y)) {
    gyro_offset_y = calibration_data.gyro_offset_y;
  }
  if (!nh.getParam("imu/calibration/gyro_offset_z", &gyro_offset_z)) {
    gyro_offset_z = calibration_data.gyro_offset_z;
  }
  if (!nh.getParam("imu/calibration/mag_offset_x", &mag_offset_x)) {
    mag_offset_x = calibration_data.mag_offset_x;
  }
  if (!nh.getParam("imu/calibration/mag_offset_y", &mag_offset_y)) {
    mag_offset_y = calibration_data.mag_offset_y;
  }
  if (!nh.getParam("imu/calibration/mag_offset_z", &mag_offset_z)) {
    mag_offset_z = calibration_data.mag_offset_z;
  }
  if (!nh.getParam("imu/calibration/mag_radius", &mag_radius)) {
    mag_radius = calibration_data.mag_radius;
  }

  // Upload parameters to IMU
  calibration_data.accel_offset_x = accel_offset_x;
  calibration_data.accel_offset_y = accel_offset_y;
  calibration_data.accel_offset_z = accel_offset_z;
  calibration_data.accel_radius = accel_radius;
  calibration_data.gyro_offset_x = gyro_offset_x;
  calibration_data.gyro_offset_y = gyro_offset_y;
  calibration_data.gyro_offset_z = gyro_offset_z;
  calibration_data.mag_offset_x = mag_offset_x;
  calibration_data.mag_offset_y = mag_offset_y;
  calibration_data.mag_offset_z = mag_offset_z;
  calibration_data.mag_radius = mag_radius;
  bno.setSensorOffsets(calibration_data);
}

void loop() {
  //===== Encoder Section =====//
  // Read if wheel encoder values
  int ir1 = digitalRead(ir_1);
  int ir2 = digitalRead(ir_2);
  int ir3 = digitalRead(ir_3);
  int ir4 = digitalRead(ir_4);

  int returnvalue1;
  int returnvalue2;

  // Determine state of each wheel
  if (ir1 == 0) {
    if (ir2 == 0) {
      state1 = 0;
      }
    else {
      state1 = 1;
    }
  }
    else {
      if (ir2 == 0) {
        state1 = 2;
      }
    else {
      state1 = 3;
    }
  }
  
  if (ir3 == 0) {
    if (ir4 == 0) { 
      state2 = 0;
    }
    else {
      state2 = 1;
    }
  }
  else {
    if (ir4 == 0) {
      state2 = 2;
    }
    else {
      state2 = 3;
    }
  }

  // Determine increment direction for each wheel
  // Wheel 1
  switch (oldstate1){
    case 0: 
      if (state1 == 1){ returnvalue1 = 1;}
      else if (state1 == 2) {returnvalue1 = -1;}
      else {returnvalue1 = 0;}
      break;
    case 1:
      if (state1 == 3){ returnvalue1 = 1;}
      else if (state1 == 0) {returnvalue1 = -1;}
      else {returnvalue1 = 0;}
      break;
    case 2:
      if (state1 == 0){ returnvalue1 = 1;}
      else if (state1 == 3) {returnvalue1 = -1;}
      else {returnvalue1 = 0;}
      break;
    case 3:
      if (state1 == 2){ returnvalue1 = 1;}
      else if (state1 == 1) {returnvalue1 = -1;}
      else {returnvalue1 = 0;}
      break;
  }
  // Wheel 2
  switch (oldstate2){
    case 0: 
      if (state2 == 1){ returnvalue2 = 1;}
      else if (state2 == 2) {returnvalue2 = -1;}
      else {returnvalue2 = 0;}
      break;
    case 1:
      if (state2 == 3){ returnvalue2 = 1;}
      else if (state2 == 0) {returnvalue2 = -1;}
      else {returnvalue2 = 0;}
      break;
    case 2:
      if (state2 == 0){ returnvalue2 = 1;}
      else if (state2 == 3) {returnvalue2 = -1;}
      else {returnvalue2 = 0;}
      break;
    case 3:
      if (state2 == 2){ returnvalue2 = 1;}
      else if (state2 == 1) {returnvalue2 = -1;}
      else {returnvalue2 = 0;}
      break;
  }
  
  // 
  if (ir1 != oldenc_1) {
    counter1 = counter1 + returnvalue1;
  }
  
  if (ir3 != oldenc_3) {
    counter2 = counter2 + returnvalue2;
  }

  // Publish pure 16-bit Int, overflow is considered in the Python code
  // counter2 increments going forward and decrements going backward
  // counter1 decrements going forward and increments going backward, so it's
  //  value must be negated before publishing.
  // Output tickk messages must be positive going forward and negative going backward.
  publish_left(counter1);
  publish_right(-counter2);

  // Update old states and encoder readings
  oldenc_1 = ir1;
  oldenc_2 = ir2;
  oldenc_3 = ir3;
  oldenc_4 = ir4;
  oldstate1 = state1;
  oldstate2 = state2;
  //===== Encoder Section =====

  //===== IMU Section =====//
  // Get IMU data from sensor
  quat = bno.getQuat(); 
  ang = bno.getVector(Adafruit_BNO055::VECTOR_GYROSCOPE);
  lin = bno.getVector(Adafruit_BNO055::VECTOR_ACCELEROMETER);
  mag = bno.getVector(Adafruit_BNO055::VECTOR_MAGNETOMETER);

  //Serial.println(mag.x());

//  // Convert to IMU array message
//  imu_data.data[0] = quat.w();
//  imu_data.data[1] = quat.x();
//  imu_data.data[2] = quat.y();
//  imu_data.data[3] = quat.z();
//  vec = bno.getVector(Adafruit_BNO055::VECTOR_GYROSCOPE);
//  imu_data.data[4] = vec.x();
//  imu_data.data[5] = vec.y();
//  imu_data.data[6] = vec.z();
//  vec = bno.getVector(Adafruit_BNO055::VECTOR_ACCELEROMETER);
//  imu_data.data[7] = vec.x();
//  imu_data.data[8] = vec.y();
//  imu_data.data[9] = vec.z();
//  vec = bno.getVector(Adafruit_BNO055::VECTOR_MAGNETOMETER);
//  imu_mag.data[0] = vec.x();
//  imu_mag.data[1] = vec.y();
//  imu_mag.data[2] = vec.z();

  // Convert to IMU array message
  imu_data.data[0] = quat.w();
  imu_data.data[1] = quat.x();
  imu_data.data[2] = quat.y();
  imu_data.data[3] = quat.z();
  imu_data.data[4] = ang.x();
  imu_data.data[5] = ang.y();
  imu_data.data[6] = ang.z();
  imu_data.data[7] = lin.x();
  imu_data.data[8] = lin.y();
  imu_data.data[9] = lin.z();

  // Convert magnetic field values to imu_mag array message
  imu_mag.data[0] = mag.x();
  imu_mag.data[1] = mag.y();
  imu_mag.data[2] = mag.z();

  // Get calibration status
  bno.getCalibration(&imu_calib_status.system, &imu_calib_status.gyro, 
                     &imu_calib_status.accel, &imu_calib_status.mag);

  // Publish message
  imu_pub.publish(&imu_data);
  mag_pub.publish(&imu_mag);
  imu_status_pub.publish(&imu_calib_status);
  //===== IMU Section =====//
  
  nh.spinOnce();
}

void publish_right(int counter)
{
  rwheel_msg.data = counter;
  rwheel_pub.publish(&rwheel_msg);
}

void publish_left(int counter)
{
  lwheel_msg.data = counter;
  lwheel_pub.publish(&lwheel_msg);
}
