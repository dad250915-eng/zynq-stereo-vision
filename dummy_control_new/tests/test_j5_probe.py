import unittest
from controller import Controller
from kinematics import fk
from j5_probe import start_probe

class ProbeTests(unittest.TestCase):
    def setup_controller(self, q=(0,-59.9,161.97,0,17.44,0)):
        sent=[]; logs=[]; c=Controller(sent.append,logs.append)
        c.enabled=True; c.joints=q; c.pose=fk(q)
        c.joints_at=c.pose_at=1.0
        c.joint_history.extend((t,q) for t in (.4,.7,1.0))
        return c,sent,logs

    def test_exact_command_and_no_other_axis_change(self):
        c,s,l=self.setup_controller();start_probe(c,1.1)
        self.assertEqual(s,['>0.000,-59.900,161.970,0.000,20.440,0.000,30'])
        self.assertIn('J5_PROBE_BEFORE',l[0])
        c.target=None
        with self.assertRaises(ValueError):start_probe(c,1.2)
        self.assertEqual(len(s),1)

    def test_reject_stale_moving_disabled_busy_limit(self):
        for condition in ('stale','moving','disabled','busy','limit'):
            with self.subTest(condition=condition):
                c,s,l=self.setup_controller()
                if condition=='stale':c.joints_at=-10
                if condition=='moving':c.joint_history[0]=(.4,(0,-59.9,161.97,0,16,0))
                if condition=='disabled':c.enabled=False
                if condition=='busy':c.target=c.pose
                if condition=='limit':
                    c,s,l=self.setup_controller((0,-59.9,161.97,0,119,0))
                with self.assertRaises(ValueError):start_probe(c,1.1)
                self.assertEqual(s,[])

    def test_stop_available(self):
        c,s,l=self.setup_controller();start_probe(c,1.1);c.stop(1.2)
        self.assertEqual(s[-1],'!STOP')
        self.assertFalse(c.j5_probe_active)

    def test_completion_logs_feedback(self):
        c,s,l=self.setup_controller();start_probe(c,1.1)
        q=c.joint_target;p=fk(q)
        for t in (1.3,1.6,1.9):
            c.query=('joints',t);c.receive('ok '+' '.join(map(str,q)),t+.01)
            c.query=('pose',t+.02);c.receive('ok '+' '.join(map(str,p)),t+.03)
        self.assertEqual(c.completed,1)
        self.assertTrue(any('J5_PROBE_AFTER' in line for line in l))

if __name__=='__main__':unittest.main()
