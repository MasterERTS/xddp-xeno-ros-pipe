#include "ListenerXddp.h"

int main(int argc, char**argv)
{
    ros::init(argc, argv, "xddp_listener");
    ListenerXDDP listener;
    ros::Rate loop_rate(10);
    ros::spin();
}