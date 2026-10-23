import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import unittest,time,math
from kinematics import fk,solve,LOW,HIGH
from controller import Controller

class AutoPoseTests(unittest.TestCase):
    def ready(self):
        sent=[];c=Controller(sent.append,lambda x:None);c.enabled=True
        c.joints=(0,-75.31,180,0,0,0);c.pose=fk(c.joints)
        c.joints_at=c.pose_at=1
        return c,sent

    def test_fk_matches_real_controller_log(self):
        self.assertLess(math.dist(fk((0,-75.31,180,0,0,0))[:3],(80.59,0,175.80)),.01)

    def test_reachable_targets_and_limits_after_rounding(self):
        current=(0,-75.31,180,0,0,0)
        for reference in ((5,-65,160,0,10,0),(15,-30,100,20,30,0),(0,-74,179,0,0,0)):
            goal=fk(reference)[:3];q,pose=solve(goal,current)
            self.assertLess(math.dist(goal,pose[:3]),.1)
            self.assertTrue(all(lo<=v<=hi for v,lo,hi in zip(q,LOW,HIGH)))

    def test_bad_or_far_input(self):
        for goal in ((float('nan'),0,0),(1000,0,0)):
            with self.assertRaises(ValueError):solve(goal,(0,-75,180,0,0,0))

    def test_joint_command_and_no_ack_completion(self):
        c,s=self.ready();goal=fk((5,-65,160,0,10,0))[:3];q,pose=solve(goal,c.joints)
        c.move_auto(goal,q,pose,c.joints,5,1.1)
        self.assertTrue(s[-1].startswith('>'));self.assertEqual(len(s),1)
        c.receive('ok',1.2);self.assertIsNotNone(c.target)
        for t in (2,3,4):
            c.query=('joints',t);c.receive('ok '+' '.join(map(str,q)),t+.05)
            c.query=('pose',t+.1);c.receive('ok '+' '.join(map(str,pose)),t+.15)
        self.assertEqual(c.completed,1)
        # A second point can execute without re-enabling.
        goal2=fk((6,-64,158,0,10,0))[:3];q2,p2=solve(goal2,q)
        c.move_auto(goal2,q2,p2,q,5,4.2)
        self.assertTrue(c.enabled);self.assertIsNotNone(c.target)

    def test_stale_or_model_mismatch_no_send(self):
        c,s=self.ready()
        with self.assertRaises(ValueError):c.check_auto_ready(4)
        c.pose=(0,0,0,0,0,0)
        with self.assertRaises(ValueError):c.check_auto_ready(1.1)
        self.assertEqual(s,[])

    def test_stopped_or_changed_during_solve_no_send(self):
        c,s=self.ready();baseline=c.joints;goal=fk((5,-65,160,0,10,0))[:3];q,pose=solve(goal,baseline)
        c.enabled=False
        with self.assertRaises(ValueError):c.move_auto(goal,q,pose,baseline,5,1.1)
        c.enabled=True;c.joints=(2,-75.31,180,0,0,0);c.pose=fk(c.joints)
        with self.assertRaises(ValueError):c.move_auto(goal,q,pose,baseline,5,1.1)
        self.assertEqual(s,[])

