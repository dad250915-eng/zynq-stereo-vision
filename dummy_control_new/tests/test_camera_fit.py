import importlib.util
from pathlib import Path
import unittest
import numpy as np

spec=importlib.util.spec_from_file_location('camera_fit',Path(__file__).resolve().parents[1]/'calibration_20261012/fit_camera_calibration.py')
fitter=importlib.util.module_from_spec(spec)
spec.loader.exec_module(fitter)


class FitTests(unittest.TestCase):
    def test_known_rotation_and_translation_direction(self):
        camera=np.array([[0,0,0],[30,0,0],[0,20,0],[0,0,40],[20,30,50]],float)
        expected_r=np.array([[0,0,1],[1,0,0],[0,1,0]],float)
        expected_t=np.array([200,-25,160],float)
        r,t=fitter.rigid_fit(camera,camera@expected_r.T+expected_t)
        np.testing.assert_allclose(r,expected_r,atol=1e-12)
        np.testing.assert_allclose(t,expected_t,atol=1e-12)

    def test_degenerate_points_rejected(self):
        planar=np.array([[0,0,0],[1,0,0],[0,1,0],[1,1,0]],float)
        with self.assertRaises(ValueError):fitter.rigid_fit(planar,planar)


if __name__=='__main__':unittest.main()
