import sys,unittest
sys.path.insert(0,'E:/dummy_control_new')
from controller import Controller
from kinematics import fk

class HandLiftTests(unittest.TestCase):
    def ready(self):
        sent=[];c=Controller(sent.append,lambda _:None);c.enabled=True
        c.joints=(10,-40,130,0,0,0);c.pose=fk(c.joints);c.joints_at=c.pose_at=1
        return c,sent
    def test_hand_waits_query_and_ack_preserves_enable(self):
        c,s=self.ready();c.query=('pose',1);c.hand(True,1)
        c.tick(1.1);self.assertEqual(s,[])
        c.receive('ok '+' '.join(map(str,c.pose)),1.2);c.tick(1.3)
        self.assertEqual(s,['!HAND_O']);self.assertIsNone(c.query)
        c.receive('ok hand open',1.4)
        self.assertTrue(c.enabled);self.assertIsNone(c.pending);self.assertEqual(c.completed,0)
        c.hand(False,1.5);c.tick(1.5);c.receive('ok hand close',1.6)
        self.assertEqual(s[-1],'!HAND_C');self.assertTrue(c.enabled)
    def test_hand_excludes_motion_and_repeated_hand(self):
        c,s=self.ready();c.hand(False,1)
        with self.assertRaises(ValueError):c.hand(True,1)
        with self.assertRaises(ValueError):c.move_vertical(100,5,1)
        c.tick(1)
        with self.assertRaises(ValueError):c.move(c.pose[:3],c.pose[3:],5,1.1)
    def test_stop_cancels_unsent_hand(self):
        c,s=self.ready();c.query=('pose',1);c.hand(False,1);c.stop(1.1)
        c.receive('Stopped ok',1.2);c.tick(1.3)
        self.assertNotIn('!HAND_C',s)
    def test_hand_timeout_no_resend(self):
        c,s=self.ready();c.hand(True,1);c.tick(1);c.tick(3.1);c.tick(5)
        self.assertIsNotNone(c.fault);self.assertEqual(s.count('!HAND_O'),1)
    def test_vertical_enforces_down_and_uses_current_z(self):
        from vertical_pose import solve_down,DOWN_ABC
        from controller import angle_error
        for delta in (10,-10):
            c,s=self.ready()
            c.joints,c.pose=solve_down((150,150,180),c.joints)
            p=c.pose;c.move_vertical(delta,5,1)
            values=list(map(float,s[-1][1:].split(',')))
            actual=fk(values[:6])
            for i in (0,1):self.assertAlmostEqual(actual[i],p[i],delta=.1)
            self.assertAlmostEqual(actual[2],p[2]+delta,delta=.1)
            self.assertLess(angle_error(actual[3:],DOWN_ABC),.2)
            self.assertTrue(s[-1].startswith('>'));self.assertIsNotNone(c.target)
            with self.assertRaises(ValueError):c.move_vertical(delta,5,1.1)
    def test_stale_or_disabled_prevents_actions(self):
        for stale in (True,False):
            c,s=self.ready()
            if stale:c.pose_at=0
            else:c.enabled=False
            with self.assertRaises(ValueError):c.hand(True,2)
            with self.assertRaises(ValueError):c.move_vertical(100,5,2)
            self.assertEqual(s,[])
if __name__=='__main__':unittest.main()
