import unittest
from pathlib import Path
import numpy as np
from camera_mode_core import load_calibration,CameraCapture

class CalibrationTests(unittest.TestCase):
    def test_new_samples_and_coordinate_version(self):
        d=load_calibration(Path(__file__).resolve().parents[1]/'camera_calibration.json')
        self.assertEqual(d['coordinate_model'],'j5-reversed-v1')
        self.assertTrue(d['pairing_confirmed'])
        self.assertEqual(d['calibration_id'],'20261012-user-7groups')
        self.assertEqual(d['robot_x_offset_mm'],10.0)
        self.assertEqual(d['labels'],list('ABCDEFG'))
        samples=np.asarray(d['camera_samples']);self.assertEqual(samples.shape,(7,1,3))
        np.testing.assert_array_equal(samples[:,0,:],[[-5,-38,406],[-5,-39,434],[-44,-38,410],[-41,-36,381],[-7,15,417],[-4,15,391],[5,-38,412]])
        np.testing.assert_array_equal(d['robot_xyz'],[[200,0,160],[200,-25,160],[170,0,155],[170,25,155],[200,0,210],[200,20,210],[220,0,160]])
        np.testing.assert_allclose(samples.mean(1),d['camera_means'])
        pred=samples.mean(1)@np.array(d['R']).T+d['t']
        error=np.linalg.norm(pred-np.array(d['robot_xyz']),axis=1)
        np.testing.assert_allclose(error,d['residual_mm'])
        self.assertAlmostEqual(float(np.sqrt(np.mean(error**2))),4.570963498019034,places=6)
        self.assertAlmostEqual(d['max_error_mm'],6.05961477845,places=6)
        self.assertAlmostEqual(d['loo_rmse_mm'],6.627629717582207,places=6)
        np.testing.assert_allclose(pred[0],[205.07791289,2.60566486,158.44640763],atol=1e-6)

    def test_each_group_flows_through_capture(self):
        d=load_calibration(Path(__file__).resolve().parents[1]/'camera_calibration.json')
        for mean,expected in zip(d['camera_means'],d['predicted_robot_xyz']):
            # Camera input accounts for the separately configured grasp offset.
            mean=np.asarray(mean)-[0,0,d['camera_depth_offset_mm']]
            capture=CameraCapture(d)
            for i in range(5):
                line=f'XYZ RIGHT_RECT frame={i:08X} valid=1 uv=(454,313) d_q4=2640 X_mm={mean[0]} Y_mm={mean[1]} Z_mm={mean[2]} n=121/121 support=1000/1000 iqr_q4=48 calc_us=1000'
                capture.feed(line,i*.6,i*.6)
            np.testing.assert_allclose(capture.check(2.5),np.asarray(expected)+[10,0,0])

    def test_direct_calibration_does_not_add_legacy_offset(self):
        d=load_calibration(Path(__file__).resolve().parents[1]/'camera_calibration.json')
        d['camera_depth_offset_mm']=0.0
        capture=CameraCapture(d)
        for i in range(5):
            capture.feed(f'XYZ RIGHT_RECT frame={i:08X} valid=1 uv=(454,313) d_q4=2640 X_mm=-5 Y_mm=-38 Z_mm=406 n=121/121 support=1000/1000 iqr_q4=48 calc_us=1000',i*.6,i*.6)
        self.assertEqual(capture.grasp_camera,capture.locked)
        np.testing.assert_allclose(capture.check(2.5),np.asarray(d['predicted_robot_xyz'][0])+[10,0,0])

    def test_invalid_offset_rejected(self):
        d=load_calibration(Path(__file__).resolve().parents[1]/'camera_calibration.json')
        for value in (float('nan'),float('inf')):
            with self.subTest(value=value):
                d['camera_depth_offset_mm']=value
                with self.assertRaises(ValueError):CameraCapture(d)

if __name__=='__main__':unittest.main()
