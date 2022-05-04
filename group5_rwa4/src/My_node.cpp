/**
 * @file My_node.cpp
 * @author Pulkit Mehta (pmehta09@umd.edu)
 * @author Darshan Jain (djain12@umd.edu)
 * @author Jeffin J K (jeffinjk@umd.edu)
 * @brief Node for RWA4
 * @version 0.1
 * @date 2022-04-13
 * 
 * @copyright Copyright (c) 2022
 * 
 */

// Copyright 2016 Open Source Robotics Foundation, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include <algorithm>
#include <vector>
#include <string>
#include <ros/ros.h>
#include <vector>

#include <nist_gear/LogicalCameraImage.h>
#include <nist_gear/Order.h>
#include <nist_gear/Proximity.h>
#include <sensor_msgs/JointState.h>
#include <sensor_msgs/LaserScan.h>
#include <sensor_msgs/PointCloud.h>
#include <sensor_msgs/Range.h>
#include <std_msgs/Float32.h>
#include <std_msgs/String.h>
#include <std_srvs/Trigger.h>
#include <trajectory_msgs/JointTrajectory.h>
#include <nist_gear/DetectedProduct.h>
#include <nist_gear/SubmitShipment.h>
#include <nist_gear/AGVToAssemblyStation.h>
#include <nist_gear/AssemblyStationSubmitShipment.h>


#include "../include/comp/comp_class.h"
#include "../include/agv/agv.h"
#include "../include/util/util.h"
#include "../include/camera/logical_camera.h"
#include "../include/arm/arm.h"


void as_submit_assembly(ros::NodeHandle & node, std::string station_id, std::string shipment_type)
{
  ros::ServiceClient client1 = node.serviceClient<nist_gear::AssemblyStationSubmitShipment>("/ariac/as1/submit_shipment");
  ros::ServiceClient client2 = node.serviceClient<nist_gear::AssemblyStationSubmitShipment>("/ariac/as2/submit_shipment");
  ros::ServiceClient client3 = node.serviceClient<nist_gear::AssemblyStationSubmitShipment>("/ariac/as3/submit_shipment");
  ros::ServiceClient client4 = node.serviceClient<nist_gear::AssemblyStationSubmitShipment>("/ariac/as4/submit_shipment");
   
  nist_gear::AssemblyStationSubmitShipment srv;
  srv.request.shipment_type = shipment_type;

  if(station_id == "as1"){
    client1.call(srv);
  }
  if(station_id == "as2"){
    client2.call(srv);
  }
  if(station_id == "as3"){
    client3.call(srv);
  }
  if(station_id == "as4"){
    client4.call(srv);
  }
}


int main(int argc, char ** argv)
{
  // Last argument is the default name of the node.
  ros::init(argc, argv, "My_node");

  ros::NodeHandle node;
  ros::AsyncSpinner spinner(0);
  spinner.start();

  ros::Time start = ros::Time::now();
  // Instance of custom class from above.
  MyCompetitionClass comp_class(node);
  comp_class.init();

  LogicalCamera cam(node);

  // create an instance of the kitting arm
  motioncontrol::Arm arm(node);
  arm.init();
  gantry_motioncontrol::Gantry gantry(node);
  gantry.init();

  // ros::Subscriber depth_camera_bins1_subscriber = node.subscribe(
  //   "/ariac/depth_camera_bins1/depth/image_raw --noarr", 10,
  //   &MyCompetitionClass::depth_camera_bins1_callback, &comp_class);

  ros::Subscriber proximity_sensor_subscriber = node.subscribe(
    "/ariac/proximity_sensor_0", 10,
     &MyCompetitionClass::proximity_sensor0_callback,&comp_class);

  ros::Subscriber break_beam_subscriber = node.subscribe(
    "/ariac/breakbeam_0", 10,
    &MyCompetitionClass::breakbeam0_callback, &comp_class);

  ros::Subscriber laser_profiler_subscriber = node.subscribe(
    "/ariac/laser_profiler_0", 10,
    &MyCompetitionClass::laser_profiler0_callback,&comp_class);
  
  ROS_INFO("Setup complete.");
  

  ros::ServiceClient client1 = node.serviceClient<nist_gear::AssemblyStationSubmitShipment>("/ariac/as1/submit_shipment");
  ros::ServiceClient client2 = node.serviceClient<nist_gear::AssemblyStationSubmitShipment>("/ariac/as2/submit_shipment");
  ros::ServiceClient client3 = node.serviceClient<nist_gear::AssemblyStationSubmitShipment>("/ariac/as3/submit_shipment");
  ros::ServiceClient client4 = node.serviceClient<nist_gear::AssemblyStationSubmitShipment>("/ariac/as4/submit_shipment");
   
  nist_gear::AssemblyStationSubmitShipment asrv;


  std::vector<Order> orders;
  std::string comp_state;
  Product remaining_part;
  std::string remaining_part_agv;
  std::string remaining_part_as;
  std::string remaining_shipment_type;
  std::vector<std::string> parts_not_found;
  std::string blackout_part_type;
  geometry_msgs::Pose blackout_part_tray_pose;
  std::string check_agv_id;
  std::vector<Product> parts_to_check_later;
  // comp_state = comp_class.getCompetitionState();
  auto competition_start_time = comp_class.getClock();

  
  // Boolean Flags
  bool order0_done = false;
  bool order1_models_found = false;
  bool remaining_found = false;
  bool remaining_shipment = false;
  bool finished = false;
  bool noblackout = true;
  bool check_later = false;
  bool notfinished = true;
  bool order1_done = false;
  bool check_order0_kits = true;
  unsigned short int id{}; 
  ros::Rate rate = 2;	  
  rate.sleep();	



  // find parts seen by logical cameras
  auto list1 = cam.findparts();  
  ros::Duration(sleep(3.0)); 

  // Finding empty bins 
  auto empty_bins_at_start = cam.get_ebin_list();
  ros::Duration(sleep(1.0));
  for(auto &bin: empty_bins_at_start){
    ROS_INFO_STREAM("Empty bin numbers: "<< bin);
  }
  
  

  // Pick parts from conveyor
  auto empty_bins = arm.pick_from_conveyor(empty_bins_at_start, 0);
  empty_bins_at_start.clear();

  gantry.goToPresetLocation(gantry.home_);
  // gantry.goToPresetLocation(gantry.near_as1_);
  // gantry.goToPresetLocation(gantry.at_agv1_as1_);
  // gantry.goToPresetLocation(gantry.near_as1_);
  // gantry.goToPresetLocation(gantry.at_as1_);
  // gantry.goToPresetLocation(gantry.near_as1_);
  // gantry.goToPresetLocation(gantry.at_agv2_as1_);
  // gantry.goToPresetLocation(gantry.near_as1_);
  // gantry.goToPresetLocation(gantry.at_as1_);
  // gantry.goToPresetLocation(gantry.near_as1_);  
  // gantry.goToPresetLocation(gantry.home_);

  // gantry.goToPresetLocation(gantry.near_as3_);
  // gantry.goToPresetLocation(gantry.at_agv3_as3_);
  // gantry.goToPresetLocation(gantry.near_as3_);
  // gantry.goToPresetLocation(gantry.at_as3_);
  // gantry.goToPresetLocation(gantry.near_as3_);
  // gantry.goToPresetLocation(gantry.at_agv4_as3_);
  // gantry.goToPresetLocation(gantry.near_as3_);
  // gantry.goToPresetLocation(gantry.at_as3_);
  // gantry.goToPresetLocation(gantry.near_as3_);  
  // gantry.goToPresetLocation(gantry.home_);




  arm.goToPresetLocation("home1");
  arm.goToPresetLocation("home2");
  gantry.goToPresetLocation(gantry.home_);
  for(auto &bin: empty_bins){
    ROS_INFO_STREAM("Empty bin after placing parts from conveyor: "<< bin);
  }  

  // find parts seen by logical cameras
  ROS_INFO_STREAM("Making List");
  auto list = cam.findparts(); 
  ROS_INFO_STREAM("Made List");
  ros::Duration(sleep(3.0));
  // Segregate parts and create the map of parts
  ROS_INFO_STREAM("Seg list");
  cam.segregate_parts(list);
  ROS_INFO_STREAM("Segd list");
  ros::Duration(sleep(3.0));
  // get the map of parts
  ROS_INFO_STREAM("Creating map");
  auto cam_map = cam.get_camera_map();
  ROS_INFO_STREAM("Created map");
  ros::Duration(sleep(3.0)); 
  
  while(ros::ok){
  
  // get the list of orders
  orders = comp_class.get_order_list();  
  // Process order 0
  if (notfinished)
  {
    if (!order0_done){
      // kitting
      ROS_INFO_STREAM(orders.at(0).kitting.size());
      for(auto &kit: orders.at(0).kitting){

        ROS_INFO_STREAM("[CURRENT PROCESS]: " << kit.shipment_type);

        // Create an empty list of parts for the kit
        std::vector<Product> parts_for_kitting;
        
        // Push all the parts in kit to the list
        for (auto &part:kit.products){
          part.processed = false;
          parts_for_kitting.push_back(part);
        }
        
        unsigned short int shipment_product_count{0};

        while(shipment_product_count <= kit.products.size()){
          if (shipment_product_count == kit.products.size()){
            break;
          }
          ROS_INFO_STREAM("SHIPMENT COUNT: " << shipment_product_count);
          // Process the shipment
          for(auto &iter: parts_for_kitting){

            // if (cam.faulty_part_list_.size() > 0 ){
            //   ROS_INFO_STREAM("Checked: part is faulty, removing it from the tray");
            //   arm.pickPart("assembly_sensor_red", cam.faulty_part_list_.at(0).world_pose);
            //   arm.goToPresetLocation("home2");
            //   arm.deactivateGripper();
            //   cam.query_faulty_cam();
            //   break;
            // }

            ROS_INFO_STREAM("[CURRENT PART BEING PROCESSED]: " << iter.type);

            if (!iter.processed){
              // Find the required part from the map of parts
              auto p = cam_map.find(iter.type);
              
              // Search the part from the map
              for (int i{0}; i < p->second.size(); i++){
                
                // Check if the part is not already picked before, i.e., is present on bin
                if(p->second.at(i).status.compare("free") == 0){
                  
                  // Check if part is in the eight bins. 
                  if ((p->second.at(i).camera.compare("logical_camera_bins0") == 0) || (p->second.at(i).camera.compare("logical_camera_bins1") == 0) ){
                    
                    // Check if the part in is the bins near to the conveyor
                    if (p->second.at(i).bin_number == 1 || p->second.at(i).bin_number == 2 || p->second.at(i).bin_number == 5 || p->second.at(i).bin_number == 6){
                      
                      ROS_INFO_STREAM("Moving the part using kitting arm: " << iter.type);
                      
                      // Check if the part is a pump
                      if(iter.type.find("pump") != std::string::npos){
                        std::array<double, 3> rpy = motioncontrol::eulerFromQuaternion(iter.frame_pose);
                        auto roll = rpy[0];
                        ROS_INFO_STREAM("Roll :" <<roll);

                        // Check is the pump is to be flipped
                        if(abs(abs(roll) - 3.14) < 0.5){ 
                          auto part = p->second.at(i);        
                          arm.flippart(part, empty_bins, iter.frame_pose, kit.agv_id, true);
                          cam_map[iter.type].at(i).status = "processed";

                        }
                        else{
                          arm.movePart(iter.type, p->second.at(i).world_pose, iter.frame_pose, kit.agv_id);
                          cam_map[iter.type].at(i).status = "processed";

                        }
                      }

                      // Part is not a pump
                      else{
                        arm.movePart(iter.type, p->second.at(i).world_pose, iter.frame_pose, kit.agv_id);
                        cam_map[iter.type].at(i).status = "processed";
                      }
                    }

                    // Part is in bins away from conveyor
                    else{
                      ROS_INFO_STREAM("Moving the part using gantry: " << iter.type);
                      
                      // Check is the part is in bins0
                      if(p->second.at(i).camera.compare("logical_camera_bins0") == 0){
                        gantry.goToPresetLocation(gantry.at_bins1234_);
                      }
                      // else, the parts are in bins1
                      else{
                        gantry.goToPresetLocation(gantry.at_bins5678_);
                      }

                      if(iter.type.find("pump") != std::string::npos) {
                        std::array<double, 3> rpy = motioncontrol::eulerFromQuaternion(iter.frame_pose);
                        auto roll = rpy[0];
                        ROS_INFO_STREAM("Roll :" << roll);
                        
                        if (abs(abs(roll) - 3.14) < 0.5){
                          auto part = p->second.at(i);
  
                          int bin_selected = 0;
                          for(auto &bin: empty_bins){
                              ROS_INFO_STREAM("bin number "<< bin);
                              if(bin == 1 || bin == 2 || bin == 5 || bin == 6)
                              {
                                  bin_selected = bin;
                                  break;
                              }
                          }
                          gantry.move_gantry_to_bin(p->second.at(i).bin_number);
                          gantry.movePartfrombin(p->second.at(i).world_pose, iter.type, bin_selected);
                          bool kitting_arm_required = false;
                          arm.flippart(part, empty_bins, iter.frame_pose, kit.agv_id, kitting_arm_required);
                          cam_map[iter.type].at(i).status = "processed";
                        }
                        
                        else{
                        gantry.move_gantry_to_bin(p->second.at(i).bin_number);
                        gantry.movePart(p->second.at(i).world_pose, iter.frame_pose, kit.agv_id, iter.type);
                        gantry.goToPresetLocation(gantry.home_);
                        cam_map[iter.type].at(i).status = "processed";
                        }

                      }

                      else{
                        gantry.move_gantry_to_bin(p->second.at(i).bin_number);
                        gantry.movePart(p->second.at(i).world_pose, iter.frame_pose, kit.agv_id, iter.type);
                        gantry.goToPresetLocation(gantry.home_);
                        cam_map[iter.type].at(i).status = "processed";
                      }
                    }
                  }
                    

                  // Check for Sensor Blackout
                  if(ros::Time::now().toSec() - comp_class.CheckBlackout() > 5){
                    ROS_INFO_STREAM("Sensor Blackout");
                    noblackout = false;
                  }
                  else{
                    noblackout = true;
                  }
                  
                  if (noblackout){
                    // HIGH PRIORITY ORDER PROCESSING
                    // Check if high priority order is announced
                    if(comp_class.high_priority_announced && !order1_done){
                      while(true){
                        auto temp_order_list = comp_class.get_order_list();
                        if(temp_order_list.size() > 1){
                          for(auto &kit1: temp_order_list.at(1).kitting){

                            ROS_INFO_STREAM("[CURRENT PROCESS order 1]: " << kit1.shipment_type);

                            // Create an empty list of parts for this kit
                            std::vector<Product> parts_for_kitting1;

                            // Push all the parts in kit to the list
                            for (auto &part:kit1.products){
                              part.processed = false;
                              parts_for_kitting1.push_back(part);
                            }

                            unsigned short int product_placed_in_shipment{0};

                            // Process the shipment
                            for(auto &iter: parts_for_kitting1){

                              if (!iter.processed){
                                // Find the required part from the map of parts
                                auto p = cam_map.find(iter.type);
                                // Search the part from the map
                                for (int i{0}; i < p->second.size(); i++){
                                  // Check if the part is not already picked before, i.e., is present on bin
                                  if(p->second.at(i).status.compare("free") == 0){
                                    // Pick and place the part from bin to agv tray
                                    arm.movePart(iter.type, p->second.at(i).world_pose, iter.frame_pose, kit1.agv_id);
                                    // Update the status of the picked up part
                                    cam_map[iter.type].at(i).status = "processed";
                                    
                                    if (noblackout){
                                      // Get the data from quality control sensors	
                                      cam.query_faulty_cam();
                                      auto faulty_list = cam.get_faulty_part_list();
                                      
                                      double outside_time = ros::Time::now().toSec();
                                      double inside_time = ros::Time::now().toSec();
                                      // Delay for list construction
                                      ROS_INFO_STREAM("entering delay");
                                      
                                      while (inside_time - outside_time < 1.0) {
                                          inside_time = ros::Time::now().toSec();
                                      }
                                      
                                      // Check if part is faulty
                                      if (cam.faulty_part_list_.size() > 1){
                                        unsigned short int id{0};
                                        if (cam.faulty_part_list_.at(0).faulty_cam_agv.compare(kit.agv_id) == 0){
                                          id = 0;
                                        }
                                        if (cam.faulty_part_list_.at(1).faulty_cam_agv.compare(kit.agv_id) == 0){
                                          id = 1;
                                        }
                                        ROS_INFO_STREAM("part is faulty, removing it from the tray size 1");
                                        arm.pickPart(iter.type, cam.faulty_part_list_.at(id).world_pose);
                                        arm.goToPresetLocation("home2");
                                        arm.deactivateGripper();
                                        cam.query_faulty_cam();
                                        continue;
                                      }
                                      if (cam.faulty_part_list_.size() == 1){
                                        ROS_INFO_STREAM("part is faulty, removing it from the tray");
                                        arm.pickPart(iter.type, cam.faulty_part_list_.at(0).world_pose);
                                        arm.goToPresetLocation("home2");
                                        arm.deactivateGripper();
                                        cam.query_faulty_cam();
                                        continue;
                                      }
                                      iter.processed = true;
                                      break;
                                    }
                                    else{
                                      break;
                                    }
                                  } 
                                }
                              }
                              product_placed_in_shipment++;
                            }
                            if(product_placed_in_shipment == kit1.products.size()){
                              ros::Duration(sleep(1.0));
                              motioncontrol::Agv agv{node, kit1.agv_id};
                              if (agv.getAGVStatus()){
                                agv.shipAgv(kit1.shipment_type, kit1.station_id);
                              }
                            }
                          }
                          order1_done = true;
                          break; 
                        }
                      }
                    }                    

                    // Get the data from quality control sensors	
                    cam.query_faulty_cam();
                    auto faulty_list = cam.get_faulty_part_list();
                    
                    double outside_time = ros::Time::now().toSec();
                    double inside_time = ros::Time::now().toSec();
                    // Delay for list construction
                    ROS_INFO_STREAM("entering delay");
                    while (inside_time - outside_time < 2.0) {
                        inside_time = ros::Time::now().toSec();
                    }
                    ROS_INFO_STREAM("Number of faulty parts in list: " << cam.faulty_part_list_.size());
                    
                    // Check if part is faulty
                    if (cam.faulty_part_list_.size() > 0 && (cam.faulty_part_list_.at(0).type.compare(iter.type) == 0)){
                      ROS_INFO_STREAM("part is faulty, removing it from the tray");
                      arm.pickPart(iter.type, cam.faulty_part_list_.at(0).world_pose);
                      arm.goToPresetLocation("home2");
                      arm.deactivateGripper();
                      cam.query_faulty_cam();
                      continue;
                    }
                    
                    iter.processed = true;
                    ROS_INFO_STREAM("Labelled as processed" << iter.type);
                    shipment_product_count++;
                    ROS_INFO_STREAM("Shipment count after processed: " << shipment_product_count);
                    break;
                  }
                  else{
                    parts_to_check_later.push_back(iter);
                    ROS_INFO_STREAM("Pushed in to check later: " << iter.type);
                    break;
                  }
                } 
              }
            }

          }
          shipment_product_count++;
        }
        ros::Duration(sleep(1.0));
        motioncontrol::Agv agv{node, kit.agv_id};
        auto agv_stat = agv.getAGVStatus();
        agv.shipAgv(kit.shipment_type, kit.station_id);
        ROS_INFO_STREAM("AGV Shipped "<< kit.agv_id);
        ROS_INFO_STREAM("Moving to next shipment");


      }
            
      for(auto &asmb: orders.at(0).assembly){
        ROS_INFO_STREAM("[CURRRENT PROCESS]: " << asmb.shipment_type);

        std::vector<Product> parts_for_assembly;
        for (auto &part:asmb.products){
          part.processed = false;
          parts_for_assembly.push_back(part);
        }

        unsigned short int shipment_product_count(0);
        while(shipment_product_count <= asmb.products.size()){
          if (shipment_product_count == asmb.products.size()){
            break;
          }
          ROS_INFO_STREAM("SHIPMENT COUNT: " << shipment_product_count);

          for(auto &iter: parts_for_assembly){
            
            ROS_INFO_STREAM(iter.type);
            if(!iter.processed){
              auto p =cam_map.find(iter.type);
              if (p != cam_map.end()){
                for (int i{0}; i < p->second.size(); i++){
                  ROS_INFO_STREAM(p->second.at(i).camera);
                }
                for(int i{0}; i < p->second.size(); i++){
                  if(p->second.at(i).camera.compare("logical_camera_agv2as1") == 0){
                    ROS_INFO_STREAM("Moving the part: " << iter.type);
                    gantry.goToPresetLocation(gantry.near_as1_);
                    gantry.goToPresetLocation(gantry.at_agv2_as1_);
                    // gantry.pickPart(p->second.at(i).world_pose);
                    // gantry.goToPresetLocation(gantry.near_as1_);
                    // gantry.goToPresetLocation(gantry.at_as1_);
                    gantry.movePart(p->second.at(i).world_pose, iter.frame_pose, asmb.stations, iter.type);
                    gantry.goToPresetLocation(gantry.home_);
                    cam_map[iter.type].at(i).status = "processed";
                  }
                }
              } 
            }
          }
        }
      }

      order0_done = true;
    }

    // orders = comp_class.get_order_list();

    // rate.sleep();
    if (orders.size()>1 && !order1_done){
      ROS_INFO_STREAM("inside order 1");
      double outside_time = ros::Time::now().toSec();
      double inside_time = ros::Time::now().toSec();
      while (inside_time - outside_time < 15.0) {
          inside_time = ros::Time::now().toSec();
      }
      // find parts seen by logical cameras
      ROS_INFO_STREAM("Finding parts");
      auto list = cam.findparts();
      ROS_INFO_STREAM("Seg list"); 
      // Segregate parts and create the map of parts
      cam.segregate_parts(list);
      ROS_INFO_STREAM("map creation");
      // get the map of parts
      auto cam_map = cam.get_camera_map();
      outside_time = ros::Time::now().toSec();
      inside_time = ros::Time::now().toSec();
      // Delay for list construction
      ROS_INFO_STREAM("entering delay");
      while (inside_time - outside_time < 5.0) {
          inside_time = ros::Time::now().toSec();
      }
      
      for(auto &asmb: orders.at(1).assembly){
        ROS_INFO_STREAM("[CURRRENT PROCESS]: " << asmb.shipment_type);

        std::vector<Product> parts_for_assembly;
        for (auto &part:asmb.products){
          part.processed = false;
          parts_for_assembly.push_back(part);
        }
        unsigned short int shipment_product_count(0);
        std::string assembly_station = asmb.stations;

        // for (int i{0}; i < cam_map.find("assembly_pump_blue")->second.size(); i++){
        //     ROS_INFO_STREAM(cam_map.find("assembly_pump_blue")->second.at(i).camera);
        // }
        // for (int i{0}; i < cam_map.find("assembly_battery_green")->second.size(); i++){
        //     ROS_INFO_STREAM(cam_map.find("assembly_battery_green")->second.at(i).camera);
        // }

        while(shipment_product_count <= asmb.products.size()){
          if (shipment_product_count == asmb.products.size()){
            break;
          }
          
          
          // ROS_INFO_STREAM("SHIPMENT COUNT: " << shipment_product_count);
          for(auto &iter: parts_for_assembly){
            // ROS_INFO_STREAM(iter.type);
            auto p = cam_map.find(iter.type);
            if (p != cam_map.end()){
              for (int i{0}; i < p->second.size(); i++){
                if (p->second.at(i).camera.find(assembly_station) != std::string::npos){
                  ROS_INFO_STREAM("Moving the part: " << iter.type);
                  gantry.move_gantry_to_assembly_station(p->second.at(i).camera);
                  gantry.movePart(p->second.at(i).world_pose, iter.frame_pose, asmb.stations, iter.type);
                  shipment_product_count++;
                  break;
                }
              }
            }
          }
        }

        ros::Duration(sleep(1.0));
        as_submit_assembly(node, asmb.stations, asmb.shipment_type);
        if((asmb.stations=="as2")||(asmb.stations=="as4"))
        {
          gantry.goToPresetLocation(gantry.home2_);
        }

        gantry.goToPresetLocation(gantry.home_);
        parts_for_assembly.clear();
      }
      order1_done = true;
      notfinished = false;
    }
    
  }


  if(comp_class.getCompetitionState() == "done"){
    comp_class.endCompetition();
  }

  if(!notfinished){
    ros::shutdown();
  }

  }
  ros::waitForShutdown();  
}
