import sys,unittest,math
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from reference_motion import plan_grasp,plan_vertical,MotionSequence,check_segment
from vertical_pose import solve_down,DOWN_ABC
from kinematics import fk
from controller import Controller,angle_error

REST=(0,-75,180,0,0,0)
class ReferenceTests(unittest.TestCase):
    def test_complete_preflight_and_final_exact_reference_point(self):
        goal=(100,100,280);plan=plan_grasp(goal,REST);steps=plan['steps']
        self.assertEqual(plan['clearance'],0)
        self.assertEqual(len(steps),2)
        self.assertEqual(steps[0]['pose'][:3],(160.,0.,270.))
        self.assertEqual(steps[1]['pose'][:3],goal)
        for s in steps:
            self.assertLess(angle_error(s['expected'][3:],DOWN_ABC),.2)
            self.assertLess(math.dist(fk(s['joints'])[:3],s['pose'][:3]),.1)
    def test_xyz_does_not_call_segment_planner(self):
        from unittest.mock import patch
        with patch('reference_motion.line_steps',side_effect=AssertionError('unexpected segmentation')):
            self.assertEqual(len(plan_grasp((160,0,270),REST)['steps']),2)
    def test_no_partial_plan_for_invalid_target(self):
        with self.assertRaises(ValueError):plan_grasp((160,160,280),REST)
    def test_vertical_does_not_reorient_or_add_an_approach(self):
        q,p=solve_down((150,150,180),REST);goal=(p[0],p[1],p[2]+10)
        plan=plan_vertical(goal,q)
        self.assertEqual(plan['kind'],'vertical');self.assertEqual(len(plan['steps']),5)
        self.assertEqual(plan['steps'][-1]['pose'][:3],goal)
        with self.assertRaises(ValueError):plan_vertical((80,0,200),REST)
    def test_feedback_gate_all_stages_and_stop(self):
        plan=plan_grasp((170,170,210),REST);sent=[];c=Controller(sent.append,lambda _:None)
        c.enabled=True;c.joints=REST;c.pose=fk(REST);c.pose_at=c.joints_at=1
        seq=MotionSequence(c,plan,5);seq.tick(1)
        self.assertEqual(len(sent),1);seq.tick(1.1);self.assertEqual(len(sent),1)
        t=2.
        while not seq.done:
            if c.target is not None:
                s=seq.steps[seq.index]
                for _ in range(3):
                    c.query=('joints',t);c.receive('ok '+' '.join(map(str,s['joints'])),t+.01)
                    c.query=('pose',t+.1);c.receive('ok '+' '.join(map(str,s['expected'])),t+.11)
                    t+=.4
            seq.tick(t)
        self.assertLess(math.dist(c.pose[:3],plan['target']),.1)
        self.assertEqual(len(sent),len(plan['steps']))
        c.stop_sent=True
        with self.assertRaises(ValueError):MotionSequence(c,plan,5).tick(t)
    def test_stale_feedback_and_recovery_do_not_send(self):
        plan=plan_grasp((170,170,210),REST);sent=[];c=Controller(sent.append,lambda _:None)
        c.enabled=True;c.joints=REST;c.pose=fk(REST);c.pose_at=c.joints_at=1
        seq=MotionSequence(c,plan,5);seq.tick(10);self.assertEqual(sent,[])
        c.recovery={};seq.tick(10);self.assertEqual(sent,[])
    def test_explicit_reenable_after_stop_allows_new_plan(self):
        c=Controller(lambda _:None,lambda _:None);c.stop(0);c.receive('Stopped ok',.1)
        c.enable(.2);c.receive('Started ok',.3)
        self.assertTrue(c.enabled);self.assertFalse(c.stop_sent)
    def test_branch_jump_is_rejected(self):
        q,p=solve_down((150,150,180),REST)
        with self.assertRaises(ValueError):check_segment(q,tuple(v+10 for v in q),p[:3],p[:3])

if __name__=='__main__':unittest.main()
