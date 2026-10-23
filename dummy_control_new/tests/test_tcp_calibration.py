import sys,unittest,tempfile
from pathlib import Path
import numpy as np
sys.path.insert(0,'E:/dummy_control_new')
from tcp_calibration import fit_tcp,validation_error,save_profile,load_profile
from controller import rotation

class Tests(unittest.TestCase):
    def poses(self):
        tip=np.array([8.,-12,95]);fixed=np.array([150,140,250.])
        return [tuple(fixed-np.array(rotation(a))@tip)+a for a in ((10,20,30),(30,10,70),(-20,40,10),(45,-25,65),(-30,-20,15),(5,65,-25))]
    def test_recover_known_offset_and_holdout(self):
        p=self.poses();r=fit_tcp(p[:5]);self.assertTrue(np.allclose(r['offset_mm'],[8,-12,95]))
        self.assertLess(validation_error(r,p[5]),1e-8)
    def test_reject_single_axis_and_inconsistent_points(self):
        p=[(0,0,0,0,0,float(v)) for v in range(0,180,30)]
        with self.assertRaises(ValueError):fit_tcp(p)
        p=self.poses();p[0]=tuple(np.array(p[0])+[100,0,0,0,0,0])
        with self.assertRaises(ValueError):fit_tcp(p)
    def test_save_requires_physical_confirmation_and_validation(self):
        p=self.poses();r=fit_tcp(p[:5])
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'tcp.json'
            with self.assertRaises(ValueError):save_profile(path,r,p[0],p[5],False)
            bad=list(p[5]);bad[0]+=20
            with self.assertRaises(ValueError):save_profile(path,r,p[0],bad,True)
            save_profile(path,r,p[0],p[5],True);self.assertTrue(np.allclose(load_profile(path)['offset_mm'],[8,-12,95]))
    def test_held_out_is_not_used_in_fit(self):
        p=self.poses();r=fit_tcp(p[:5]);bad=list(p[5]);bad[0]+=2
        self.assertAlmostEqual(validation_error(r,bad),2)
        self.assertTrue(np.allclose(r['offset_mm'],[8,-12,95]))
if __name__=='__main__':unittest.main()
