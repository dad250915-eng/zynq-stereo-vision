"""Fit user correspondences in mm; offline only, no serial or robot access."""
import argparse
import json
from pathlib import Path
import numpy as np


def rigid_fit(camera, robot):
    camera = np.asarray(camera, dtype=float)
    robot = np.asarray(robot, dtype=float)
    if (camera.ndim != 2 or camera.shape[1] != 3 or camera.shape != robot.shape
            or len(camera) < 4 or not np.isfinite(camera).all()
            or not np.isfinite(robot).all()):
        raise ValueError('Expected at least four finite corresponding 3D points')
    if min(np.linalg.matrix_rank(camera-camera.mean(0)),
           np.linalg.matrix_rank(robot-robot.mean(0))) < 3:
        raise ValueError('Calibration points must span three dimensions')
    u, _, vt = np.linalg.svd((camera-camera.mean(0)).T @ (robot-robot.mean(0)))
    rotation = vt.T @ np.diag([1., 1., np.linalg.det(vt.T @ u.T)]) @ u.T
    return rotation, robot.mean(0)-rotation @ camera.mean(0)


def build(data, depth_offset_mm):
    x = np.asarray(data['camera_points'], dtype=float)
    y = np.asarray(data['robot_points'], dtype=float)
    rotation, translation = rigid_fit(x, y)
    if not np.isfinite(depth_offset_mm):
        raise ValueError('Depth offset must be finite')
    predicted = x @ rotation.T + translation
    errors = np.linalg.norm(predicted-y, axis=1)
    loo = []
    for i in range(len(x)):
        keep = np.arange(len(x)) != i
        r, t = rigid_fit(x[keep], y[keep])
        loo.append(float(np.linalg.norm(r @ x[i]+t-y[i])))
    return dict(
        coordinate_model=data['coordinate_model'], pairing_confirmed=True,
        calibration_id=data['calibration_id'], method='rigid_kabsch_equal_group_mean',
        labels=data['labels'], units='mm', camera_samples=x[:, None, :].tolist(),
        robot_xyz=y.tolist(), camera_means=x.tolist(), R=rotation.tolist(),
        t=translation.tolist(), predicted_robot_xyz=predicted.tolist(),
        residual_mm=errors.tolist(), rmse_mm=float(np.sqrt(np.mean(errors**2))),
        max_error_mm=float(errors.max()), loo_mm=loo,
        loo_rmse_mm=float(np.sqrt(np.mean(np.asarray(loo)**2))), loo_max_mm=max(loo),
        camera_depth_offset_mm=float(depth_offset_mm),
        status='experimental_not_independently_validated', source=data['source'],
        physical_point='User supplied paired robot and camera coordinates. Robot attitude and repeatability were not supplied; no new TCP or attitude calibration inferred.',
        transform_convention='robot_xyz = R @ (raw_camera_xyz + [0,0,camera_depth_offset_mm]) + t; R,t fitted to supplied camera points without applying the grasp offset',
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, help='Explicit JSON output; omitted means read-only preview')
    parser.add_argument('--depth-offset-mm', type=float, required=True)
    args = parser.parse_args()
    samples = json.loads((Path(__file__).parent/'samples_confirmed.json').read_text(encoding='utf-8'))
    result = build(samples, args.depth_offset_mm)
    if args.output:
        args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
