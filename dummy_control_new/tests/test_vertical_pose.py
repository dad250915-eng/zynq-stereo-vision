import sys,unittest,math
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from vertical_pose import solve_down,validate_down,DOWN_ABC,GRIPPER_AXIS
from controller import Controller,rotation,angle_error
from kinematics import fk

class VerticalTests(unittest.TestCase):
    def ready(self):
        sent=[];c=Controller(sent.append,lambda _:None);c.enabled=True
        c.joints=(0,-75,180,0,0,0);c.pose=fk(c.joints);c.pose_at=c.joints_at=1
        return c,sent
    def test_tool_axis_and_fixed_heading(self):
        r=rotation(DOWN_ABC)
        for row,v in zip(r,(0,0,-1)):
            self.assertAlmostEqual(sum(a*b for a,b in zip(row,GRIPPER_AXIS)),v)
        for goal in ((140,230,200),(200,180,200),(170,170,210),(100,100,280)):
            q,p=solve_down(goal,(0,-75,180,0,0,0))
            self.assertLess(math.dist(goal,p[:3]),.1)
            self.assertLess(angle_error(p[3:],DOWN_ABC),.2)
    def test_tilted_solution_rejected_before_send(self):
        c,s=self.ready()
        with self.assertRaises(ValueError):c.move_down(c.pose[:3],c.joints,c.pose,c.joints,5,1)
        self.assertEqual(s,[])
    def test_bad_goal_no_fallback(self):
        for g in ((float('nan'),0,0),(1000,0,0),(0,0,1000)):
            with self.assertRaises(ValueError):solve_down(g,(0,-75,180,0,0,0))
    def test_position_and_joints_alone_cannot_complete(self):
        c,s=self.ready();goal=(170,170,210);q,p=solve_down(goal,c.joints)
        c.move_down(goal,q,p,c.joints,5,1)
        for t in (2,3,4):
            c.query=('joints',t);c.receive('ok '+' '.join(map(str,q)),t+.05)
            c.query=('pose',t+.1);c.receive('ok '+' '.join(map(str,p[:3]+(10,0,0))),t+.15)
        self.assertEqual(c.completed,0);self.assertIsNotNone(c.target)
        for t in (5,6,7):
            c.query=('joints',t);c.receive('ok '+' '.join(map(str,q)),t+.05)
            c.query=('pose',t+.1);c.receive('ok '+' '.join(map(str,p)),t+.15)
        self.assertEqual(c.completed,1)
    def test_previous_up_pointing_target_rejected(self):
        from staged_motion import solve_pose
        c,s=self.ready();goal=(170,170,210)
        q,p=solve_pose(goal+(180,0,0),c.joints)
        with self.assertRaises(ValueError):c.move_down(goal,q,p,c.joints,5,1)
        self.assertEqual(s,[])
    def test_no_offset(self):
        goal=(150,150,180);q,p=solve_down(goal,(0,-75,180,0,0,0))
        self.assertLess(math.dist(goal,fk(q)[:3]),.1)

class BranchRegressionTests(unittest.TestCase):
    def test_reference_attitude_restores_reported_point(self):
        q,p=solve_down((100,100,280),(0,-75,180,0,0,0))
        validate_down((100,100,280),q)
        self.assertAlmostEqual(q[0],45,delta=.01)
    def test_high_far_targets_still_rejected(self):
        for goal in ((160,160,280),(180,120,300)):
            with self.assertRaises(ValueError):solve_down(goal,(0,-75,180,0,0,0))
    def test_branch_roundtrip(self):
        from vertical_pose import down_branches
        for q in ((30,20,90,0,70,30),(-50,30,65,0,85,-50),(120,0,110,0,70,120)):
            p=fk(q);self.assertLess(angle_error(p[3:],DOWN_ABC),1e-5)
            self.assertTrue(down_branches(p[:3]))
            solved,predicted=solve_down(p[:3],(0,-75,180,0,0,0))
            validate_down(p[:3],solved)

if __name__=='__main__':unittest.main()
