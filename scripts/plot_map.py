import sys
import os
import time
import argparse
import logging
from pathlib import Path

logging.basicConfig(level=logging.INFO)
# a little hack because we have no package
THIS_DIR = os.path.dirname(__file__)
sys.path.append(THIS_DIR)
MAP_DIR = Path(THIS_DIR) / '../map'

import numpy as np

from open3d_util import init_vis, update_points


def gather_features(files):
    all_files = list(map(lambda f: str(f), MAP_DIR.glob(files)))
    all_files.sort(key=lambda f: int(''.join(filter(str.isdigit, f))))
    features = dict()
    for fpath in all_files:
        ts = int(Path(fpath).stem.split('_')[2])
        empty_file = os.stat(fpath).st_size == 0
        with open(fpath) as f:
            if not empty_file:
                data = np.loadtxt((x.replace(':',', ') for x in f), delimiter=',')
            else:
                data = None
            features[ts] = data
        # break

    return features

def parse_args():
    parser = argparse.ArgumentParser(description='Display features in map')
    parser.add_argument('--keep', type=int, default=1, help='How many frame to keep')
    parser.add_argument('--all', help='Plot all features', action='store_true')

    args = parser.parse_args()
    return args

def main():
    all_features = gather_features('all_features_*.txt')
    instate_features = gather_features('instate_features_*.txt')
    args = parse_args()

    vis, pcd = init_vis()
    for ts, is_feature in instate_features.items():
        if is_feature is not None:
            update_points(pcd, is_feature[:, 1:])

        if args.all:
            all_feature = all_features[ts]
            if all_feature is not None:
                current_points = np.asarray(pcd.points)
                
                current_points = np.vstack((current_points, all_feature[:, 1:]))
                update_points(pcd, current_points)

        print(ts)
        vis.update_geometry()
        
            
        vis.poll_events()
        vis.update_renderer()
        time.sleep(1.0/60.0)
    input("Press key to exit")

if __name__ == "__main__":
    main()