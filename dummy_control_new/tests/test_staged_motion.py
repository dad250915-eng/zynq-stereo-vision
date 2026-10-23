import sys,unittest,math
sys.path.insert(0,'E:/dummy_control_new')
from staged_motion import solve_pose,build_plan,Sequence
from kinematics import fk
from controller import Controller,angle_error

class Tests(unittest.TestCase):
    def ready(self):
        sent=[];c=Controller(sent.append,lambda _:None);c.enabled=True
        c.joints=(20,-30,110,10,-30,5);c.pose=fk(c.joints);c.pose_at=c.joints_at=1
        return c,sent
    def test_full_pose_solver(self):
        q=(20,-30,110,10,-30,5);p=fk(q)
        solved,out=solve_pose(p,(21,-31,111,11,-31,6))
        self.assertLess(math.dist(p[:3],out[:3]),.1);self.assertLess(angle_error(p[3:],out[3:]),.2)
    def test_unreachable(self):
        with self.assertRaises(ValueError):solve_pose((2000,0,0,0,0,0),(0,-40,130,0,0,0))
    def test_plan_final_and_fixed_orientation(self):
        q=(20,-30,110,10,-30,5);p=fk(q)
        steps=build_plan(p[:3],q,p[3:],5,2,(0,0,0),False)
        self.assertEqual(len(steps),3);self.assertEqual(steps[-1]['pose'],p)
        self.assertLess(angle_error(steps[1]['pose'][3:],steps[2]['pose'][3:]),.001)
    def test_tool_offset(self):
        import numpy as np
        from controller import rotation
        q=(20,-30,110,10,-30,5);p=fk(q);offset=(0,0,20)
        tip=np.array(p[:3])+np.array(rotation(p[3:]))@offset
        steps=build_plan(tip,q,p[3:],5,2,offset,False)
        self.assertLess(math.dist(steps[-1]['pose'][:3],p[:3]),.00001)
    def test_sequence_waits_for_feedback_and_fault_stops(self):
        c,s=self.ready();q=(21,-30,110,10,-30,5);p=fk(q)
        step=dict(name='test',pose=p,joints=q,expected=p)
        seq=Sequence(c,[step,step],5);seq.tick(1);self.assertEqual(len(s),1)
        c.receive('ok',1.1);seq.tick(1.2);self.assertEqual(len(s),1);self.assertEqual(seq.index,0)
        for t in (2,3,4):
            c.query=('joints',t);c.receive('ok '+' '.join(map(str,q)),t+.01)
            c.query=('pose',t+.1);c.receive('ok '+' '.join(map(str,p)),t+.11)
        seq.tick(4.2);seq.tick(4.3);self.assertTrue(seq.done);self.assertEqual(len(s),1)
        c.fault='broken'
        with self.assertRaises(ValueError):Sequence(c,[step],5).tick(4.4)
    def test_recovery_never_advances(self):
        c,s=self.ready();seq=Sequence(c,[],5);c.recovery={};seq.tick(1);self.assertFalse(seq.done);self.assertEqual(s,[])
if __name__=='__main__':unittest.main()
