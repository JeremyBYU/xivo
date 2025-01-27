// Author: Xiaohan Fei
#include "unistd.h"
#include <algorithm>
#include <csignal>
#include <thread>
#include <future>

#include "gflags/gflags.h"
#include "glog/logging.h"
#include "opencv2/highgui/highgui.hpp"

#include "estimator.h"
#include "estimator_process.h"
#include "metrics.h"
#include "tracker.h"
#include "loader.h"
#include "viewer.h"
#include "visualize.h"

// flags
DEFINE_string(cfg, "cfg/vio.json",
              "Configuration file for the VIO application.");
DEFINE_string(root, "/home/feixh/Data/tumvi/exported/euroc/512_16/",
              "Root directory containing tumvi dataset folder.");
DEFINE_string(dataset, "xivo", "xivo | euroc | tumvi");
DEFINE_string(seq, "room1", "Sequence of TUM VI benchmark to play with.");
DEFINE_int32(cam_id, 0, "Camera id.");
DEFINE_string(out, "map/out_state", "Output file path.");
DEFINE_bool(wait_keypress, false, "Wait for keypress");

using namespace xivo;
using namespace std::literals;

bool waiting = true;

void signalHandler( int signum ) {
   std::cout << "Interrupt signal (" << signum << ") received.\n";

   // cleanup and close up stuff here  
   // terminate program  

  waiting = false;
}



int main(int argc, char **argv) {
  google::InitGoogleLogging(argv[0]);
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  signal(SIGINT, signalHandler); 

  auto cfg = LoadJson(FLAGS_cfg);
  bool verbose = cfg.get("verbose", false).asBool();

  std::string image_dir, imu_dir, mocap_dir;
  std::tie(image_dir, imu_dir, mocap_dir) =
      GetDirs(FLAGS_dataset, FLAGS_root, FLAGS_seq, FLAGS_cam_id);

  std::unique_ptr<DataLoader> loader(new DataLoader{image_dir, imu_dir});

  // create estimator
  // auto est = std::make_unique<Estimator>(
  //     LoadJson(cfg["estimator_cfg"].asString()));
  auto est = CreateSystem(
      LoadJson(cfg["estimator_cfg"].asString()));

  // create viewer
  std::unique_ptr<Viewer> viewer;
  if (cfg.get("visualize", false).asBool()) {
    viewer = std::make_unique<Viewer>(
        LoadJson(cfg["viewer_cfg"].asString()), FLAGS_seq);
  }
  // setup I/O for saving results
  if (std::ofstream ostream{FLAGS_out, std::ios::out}) {

    std::vector<msg::Pose> traj_est;

    for (int i = 0; i < loader->size(); ++i) {
      auto raw_msg = loader->Get(i);

      if (verbose && i % 1000 == 0) {
        std::cout << i << "/" << loader->size() << std::endl;
      }
      if (auto msg = dynamic_cast<msg::Image *>(raw_msg)) {
        auto image = cv::imread(msg->image_path_);
        // std::cout  << "Got Image" << std::endl; 
        est->VisualMeas(msg->ts_, image);
        if (viewer) {
          viewer->Update_gsb(est->gsb());
          viewer->Update_gsc(est->gsc());

          cv::Mat disp = Canvas::instance()->display();

          if (!disp.empty()) {
            LOG(INFO) << "Display image is ready";
            viewer->Update(disp);
            viewer->Refresh();
          }

          if (FLAGS_wait_keypress)
          {
              // Execute lambda asyncronously.
              auto f = std::async(std::launch::async, [] {
                  char temp = 'x';
                  std::cin.ignore();
                  if (std::cin.get(temp)) return temp;
              });

              // Continue execution in main thread.
              while(f.wait_for(30ms) != std::future_status::ready && waiting) {
                  // std::cout << "still waiting..." <<  waiting << std::endl;
                  viewer->Update(disp);
                  viewer->Refresh();
              }
          }
        }
      } else if (auto msg = dynamic_cast<msg::IMU *>(raw_msg)) {
        // std::cout << "Got IMU; GYRO: " <<  msg->gyro_(0,1) << ", " <<  msg->gyro_(1,1) << ", "  <<  msg->gyro_(2,1) << std::endl; 
        est->InertialMeas(msg->ts_, msg->gyro_, msg->accel_);
        // if (viewer) {
        //   viewer->Update_gsb(est->gsb());
        //   viewer->Update_gsc(est->gsc());
        // }
      } else {
        LOG(FATAL) << "Invalid entry type.";
      }

      traj_est.emplace_back(est->ts(), est->gsb());
      ostream << StrFormat("%ld", est->ts().count()) << " "
        << est->gsb().translation().transpose() << " "
        << est->gsb().rotation().log().transpose() << std::endl;

      if (!waiting)
      {
        break;
      }

      // std::this_thread::sleep_for(std::chrono::milliseconds(3));

    }
  } else {
    LOG(FATAL) << "failed to open output file @ " << FLAGS_out;
  }
  while (viewer && waiting) {
    viewer->Refresh();
    usleep(30);
  }
}
