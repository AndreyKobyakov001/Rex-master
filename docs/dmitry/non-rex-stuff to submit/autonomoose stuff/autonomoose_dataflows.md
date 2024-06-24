# Current state of data flows in Autonomoose callbacks

Note: the results here are tracking dataflows that result from one pass through a callback's control flow; data flows that rely on the
callback being invoked multiple times in a row, or two callbacks being invoked in succession, are not included.

There are 26 callback-subscriber pairs
```
match p=()-[:pubVar]->()-[:pubTarget]->()
return p
```

Out of these, there are 22 unique callbacks:

```
match p=()-[:pubVar]->()-[:pubTarget]->()<-[:contain]-(cb:cFunction)
return distinct cb.id;

decl;basic_system_supervisor;basic_system_supervisor::BasicSystemSupervisorNodelet::navPtsAvailCb(const std_msgs::Bool::ConstPtr &,);;
decl;command_checking;command_checking::CommandCheckingNodelet::readControlCommandsCb(const anm_msgs::ControlCommands &,);;
decl;command_checking;command_checking::CommandCheckingNodelet::readVehicleStateCb(const anm_msgs::VehicleState &,);;
decl;local_planner;local_planner::LocalPlannerNodelet::globalPathCb(const nav_msgs::PathConstPtr &,);;
decl;local_planner;local_planner::LocalPlannerNodelet::gridMapCb(const nav_msgs::OccupancyGridConstPtr &,);;
decl;local_planner;local_planner::LocalPlannerNodelet::obstacleListCb(const anm_msgs::DynamicObstacleListConstPtr &,);;
decl;route_publisher;route_publisher::RoutePublisherNodelet::refGPSCb(const sensor_msgs::NavSatFixConstPtr &,);;
decl;route_publisher;route_publisher::RoutePublisherNodelet::routeGoalCb(const std_msgs::StringConstPtr &,);;
decl;route_publisher;route_publisher::RoutePublisherNodelet::routeNetworkCb(const geographic_msgs::RouteNetworkConstPtr &,);;
decl;route_publisher;route_publisher::RoutePublisherNodelet::vehicleStateCb(const anm_msgs::VehicleStateConstPtr &,);;
decl;shuttle_manager;shuttle_manager::ShuttleManagerNodelet::LastWaypointCb(const std_msgs::String::ConstPtr &,);;
decl;shuttle_manager;shuttle_manager::ShuttleManagerNodelet::ShuttleConfirmationCb(const std_msgs::Empty::ConstPtr &,);;
decl;shuttle_manager;shuttle_manager::ShuttleManagerNodelet::ShuttleRequestCb(const anm_msgs::ShuttleRequest::ConstPtr &,);;
decl;v2x_stop_sign_publisher;v2x_stop_sign_publisher::V2XStopSignPublisherNodelet::refEKFOdomDatumCb(const sensor_msgs::NavSatFixConstPtr &,);;
decl;v2x_stop_sign_publisher;v2x_stop_sign_publisher::V2XStopSignPublisherVizNodelet::v2xStopSignListCb(const anm_msgs::V2XStopSignList &,);;
decl;vehicle_control;vehicle_control::VehicleControlNodelet::CmdSpeedCb(const geometry_msgs::Twist::ConstPtr &,);;
decl;vehicle_control;vehicle_control::VehicleControlNodelet::PathCb(const nav_msgs::Path &,);;
decl;vehicle_control;vehicle_control::VehicleControlNodelet::PoseCb(const anm_msgs::VehicleState &,);;
decl;vehicle_control;vehicle_control::VehicleControlNodelet::SysStateCb(const anm_msgs::SystemState &,);;
decl;vehicle_control;vehicle_control::VehicleControlNodelet::WaypointCb(const anm_msgs::NearestAnmWaypoint &,);;
decl;waypoints_collection;waypoints_collection::WaypointsCollectionNodelet::odomDatumCb(const sensor_msgs::NavSatFixConstPtr &,);;
decl;waypoints_collection;waypoints_collection::WaypointsCollectionNodelet::vehicleStateCb(const anm_msgs::VehicleStateConstPtr &,);;
```

Out of these, two of the callbacks don't have a varWrite stemming from their parameter.

```
match p=()-[:pubVar]->()-[:pubTarget]->(param)<-[:contain]-(cb:cFunction)
    where not (param)-[:varWrite]->()
return distinct cb.id

decl;shuttle_manager;shuttle_manager::ShuttleManagerNodelet::LastWaypointCb(const std_msgs::String::ConstPtr &,);;
- parameter used only in control flow
decl;shuttle_manager;shuttle_manager::ShuttleManagerNodelet::ShuttleConfirmationCb(const std_msgs::Empty::ConstPtr &,);;
- unused (commented out) parameter
```

Out of the remaining 20, only 7 transitively call a function which performs a publish:

```
match ()-[r:varWrite]->(:rosPublisher)
match ()-[:pubVar]->()-[:pubTarget]->(cbv)<-[:contain]-(cb:cFunction)-[:call*0..]->(g)
where g.id in r.containingFuncs and (cbv)-[:varWrite]->()
return distinct cb.id as cb

decl;command_checking;command_checking::CommandCheckingNodelet::readControlCommandsCb(const anm_msgs::ControlCommands &,);;
decl;route_publisher;route_publisher::RoutePublisherNodelet::routeGoalCb(const std_msgs::StringConstPtr &,);;
decl;route_publisher;route_publisher::RoutePublisherNodelet::routeNetworkCb(const geographic_msgs::RouteNetworkConstPtr &,);;
decl;route_publisher;route_publisher::RoutePublisherNodelet::vehicleStateCb(const anm_msgs::VehicleStateConstPtr &,);;
decl;shuttle_manager;shuttle_manager::ShuttleManagerNodelet::ShuttleRequestCb(const anm_msgs::ShuttleRequest::ConstPtr &,);;
decl;v2x_stop_sign_publisher;v2x_stop_sign_publisher::V2XStopSignPublisherNodelet::refEKFOdomDatumCb(const sensor_msgs::NavSatFixConstPtr &,);;
decl;v2x_stop_sign_publisher;v2x_stop_sign_publisher::V2XStopSignPublisherVizNodelet::v2xStopSignListCb(const anm_msgs::V2XStopSignList &,);;
```

Out of these, 5 have a dataflow that links the callback parameter to a publish.

Combined, the 5 callbacks contain 10 dataflows from parameter to publish. 5 of these are false positives, but due to Rex's
`varWrite` greediness due to the class variable problem. Out of the remaining 5, 2 are valid and caught. 3 are missed entirely
by Rex due to at least one broken link in the data flow chain.

Note: the snippets shown here have code (such as conditional structures) and comments removed if they're not useful for
visualizing the data flows, and some formatting has been tweaked.


```
void CommandCheckingNodelet::readControlCommandsCb(const anm_msgs::ControlCommands &control_cmds_msg) {
    anm_msgs::ControlCommands checked_cmds = control_cmds_msg;

    // this method doesn't actually output data related to `checked_cmds`, but the varWrite is included due to class variable problem
    // And the `checkResult` contains a publish. So there is a false positive flow here, but it's unrelated to the CFG:
    // checked_cmds -> command_checking_ -> throttle_result -> checkResult::param
    int throttle_result =
      this->command_checking_.checkThrottleCommand(checked_cmds.throttle_cmd);
    if (checkResult("Throttle", throttle_result) != 0) {
        checked_cmds.throttle_EN = 0;
    }

    // similar false positive flow as above

    int brake_result =
      this->command_checking_.checkBrakeCommand(checked_cmds.brake_cmd);
    if (checkResult("Brake", brake_result) != 0) {
        checked_cmds.brake_EN = 0;
    }

    // similar false positive flow as above

    int steering_result = this->command_checking_.checkSteeringCommand(
      checked_cmds.steering_pos_cmd);
    if (checkResult("Steering", steering_result) != 0) {
        checked_cmds.steering_EN = 0;
    }

    // similar false positive flow as above

    // This block also introduces a fifth false positive flow:
    // control_cmds_msg -> checked_cmds
    // -> command_checking_ // happens in (1), but also in all similar calls above
    // -> checked_cmds // happens in (2)
    // -> commands_pub // (3)


    int gear_result =
      this->command_checking_.checkGearCommand(checked_cmds.gear_cmd.gear); // (1)
    if (checkResult("Gear", gear_result) != 0) {
        checked_cmds.gear_cmd.gear = this->command_checking_.gears_.none; // (2)
    }

    // There is a valid dataflow though, and it is captured: `control_cmds_msg -> checked_cmds -> commands_pub_`
    commands_pub_.publish(checked_cmds); // (3)
}
```

- 1 valid publish dataflow tracked correctly
- 5 false positives tracked. However, these aren't invalid via the CFG, rather they result from greedy `varWrite`s by Rex


```
msg -> point
-> stored_waypoint ~~> waypoints_map ~~> this->anm_waypoints // ~~> = through reference
-> nearest // happens within the call, and `nearest` is returned by reference parameter(5)
-> this->nearest_waypoint_msg // (6)
-> last_waypiont_msg // (7)
-> this->last_waypoint_pub // (8)


void RoutePublisherNodelet::routeNetworkCb(const geographic_msgs::RouteNetworkConstPtr &msg) {

    auto &waypoints_map = this->navigator.waypointsMap(); // (1), returns `this->anm_waypoints`

    for (const auto &point : msg->points) { // (2)
        ...
            auto &stored_waypoint = waypoints_map[...]; // (3)
            stored_waypoint.setUid(unique_id::fromMsg(point.id)); // (4)
    }
    ...
    this->updatePath();
}
void RoutePublisherNodelet::updatePath() {
    ...
        update_needed += this->updateLocalization();
    ...
}
bool RoutePublisherNodelet::updateLocalization() {

    ...
    const bool waypoint_in_range = this->navigator.updateLocalization(
      vehicle_pos, this->waypoint_radius, nearest, nearest_distance); // (5)

    this->nearest_waypoint_msg.name = nearest; // (6)
    ...
        std_msgs::String last_waypoint_msg;
        last_waypoint_msg.data = nearest; // (7)
        this->last_waypoint_pub.publish(last_waypoint_msg); // (8)

```
- 1 missed data flow to a publish because data is being returned by parameter reference (`nearest`)

```

1 dataflow:
msg -> request -> this->shuttle_requests -> this->current_shuttle_request -> publishNewRouteGoal::goal_point ->
publishNewRouteGoal::msg -> route_goal_publisher

void ShuttleManagerNodelet::ShuttleRequestCb(
  const anm_msgs::ShuttleRequest::ConstPtr &msg) {
    anm_msgs::ShuttleRequest request(*msg);         // write (copyctor + deref)

    this->shuttle_requests.push(request);           // write by push

    this->processNextShuttleRequest();      // continues here
}
void ShuttleManagerNodelet::processNextShuttleRequest() {
    this->current_shuttle_request = this->shuttle_requests.front(); // write by vector::front() -- FIXED
    this->publishNewRouteGoal(this->current_shuttle_request.pickup_waypoint);   // publish!
```
- 1 valid dataflow caught


```

All pieces of the dataflow from `odom_datum` to `this->stop_slign_list_pub` are caught except for (3), which
breaks due to lack of support for cross-module function calls as well as data being returned through parameter return pointer.

void V2XStopSignPublisherNodelet::refEKFOdomDatumCb(
  const sensor_msgs::NavSatFixConstPtr &odom_datum) {
    this->ekf_odom_datum = *odom_datum; // (1)

    this->publishStopSigns();
}
void V2XStopSignPublisherNodelet::publishStopSigns() {

    anm_msgs::V2XStopSignList stop_sign_list_msg;

    ...
        sensor_msgs::NavSatFix stop_point_nav;
        stop_point_nav.altitude = this->ekf_odom_datum.altitude;   // (2)

        geometry_msgs::Point stop_point_odom;
        wave_spatial_utils::fix_to_point(stop_point_nav, this->ekf_odom_datum, &stop_point_odom); // (3) cross-module function call -- MISSED
                                                                                                  //     stop_point_nav written into `&stop_point_odom`,
                                                                                                  //     which is a return pointer

        anm_msgs::V2XStopSign stop_sign_msg;
        stop_sign_msg.position = stop_point_odom; // (4)

        stop_sign_list_msg.stopsigns.push_back(stop_sign_msg); // (5)


    this->stop_sign_list_pub.publish(stop_sign_list_msg); // publish! // (6)
}
```
- 1 missed


```
Same as above, missing (2) due to missed return parameter write.

void V2XStopSignPublisherVizNodelet::v2xStopSignListCb(const anm_msgs::V2XStopSignList &stop_sign_list) {
    visualization_msgs::MarkerArray stop_sign_list_markers;

    for (const auto &stop_sign : stop_sign_list.stopsigns) { // (1)
        this->addStopSignMarkersFromMessage(stop_sign, ..., &stop_sign_list_markers);    // (2) missing write from `stop_sign` into
                                                                                         // `stop_sign_list_markers`, which is a return parameter
    }
    this->stop_sign_list_viz_pub.publish(stop_sign_list_markers); // (3)
}
```
- 1 missed



## Some interesting dataflows observed in other callbacks

- Some of these are captured by the new dataflow extractor, some still aren't.

```
void LocalPlannerNodelet::gridMapCb(const nav_msgs::OccupancyGridConstPtr &gridmap) {
    this->OGM_msgs_ = *gridmap;                     // assign with overloaded deref
    opt_local_planner::MatrixXc map(...);

    std::vector<int8_t>::iterator mapDataIter = this->OGM_msgs_.data.begin();  // getting iterator from container (captured by new data flow extractor)
    for (size_t j_y = 0; j_y < this->OGM_msgs_.info.height; ++j_y) {
        for (size_t i_x = 0; i_x < this->OGM_msgs_.info.width; ++i_x) {
            map(i_x, j_y) = static_cast<char>(*mapDataIter);                   // assign to matrix using `operator()()` -- MISSED (not yet implemented)
            ++mapDataIter;
        }
    }
    ...
}
```

```
void CommandCheckingNodelet::readVehicleStateCb(const anm_msgs::VehicleState &state_msg) {
    VehicleState new_state;

    // writes through function return (captured by new data flow extractor)

    new_state.lin_velocity = convertVector3(state_msg.velocity.linear);
    new_state.ang_velocity = convertVector3(state_msg.velocity.angular);
    new_state.lin_acceleration = convertVector3(state_msg.acceleration.linear);
    new_state.ang_acceleration = convertVector3(state_msg.acceleration.angular);
    ...
}
```

```
void VehicleControlNodelet::PathCb(const nav_msgs::Path &msg) {
    float x, y, heading, vel_path;

    for (unsigned int i = 0; i < msg.poses.size(); ++i) {
        tf::Quaternion q;
        double roll, pitch, yaw;

        // Another occurrence of the pattern in Autonomoose where a bunch of data is passed to a function, along
        // with a trailing return parameter, and the data is written to the reference parameter.
        // See the publish analysis above, where this pattern occurs a couple of times and prevents us from capturing
        // valid dataflows.

        tf::quaternionMsgToTF(msg.poses[i].pose.orientation, q);    // write msg to q through param reference -- MISSED

        // This is a very interesting data flow... q -> roll, pitch, yaw
        // `tf::Matrix3x3` here is a library type's constructor. It is being constructed in-place, and then the data
        // from the temporary object is being written out into reference parameters.
        // The problem is that we don't visit library functions, so we can't know all this... depending on how crucial this pattern is,
        // can potentially whitelist "library member method invoked with nothing but return parameters, no return value"...

        tf::Matrix3x3(q).getRPY(roll, pitch, yaw);                  // write q to roll,pitch,yaw through param reference -- MISSED

        // that's quite some indirection you've got there (all shown varWrites are caught)

        x = msg.poses[i].pose.position.x;   // msg -> x
        y = msg.poses[i].pose.position.y;   // msg -> y
        vel_path = msg.poses[i].pose.position.z; // msg -> vel_path

        this->veh_ctrl.path_x.push_back(x); // x -> veh_ctrl
        this->veh_ctrl.path_y.push_back(y); // y -> veh_ctrl
        this->veh_ctrl.path_vel.push_back(vel_path); // vel_path -> veh_ctrl
    }
}
```

```
// This is one of the functions which had a missed publisher dataflow, analyzed above, but this snippet
// isn't directly involved in that particular dataflow:

void RoutePublisherNodelet::routeNetworkCb(const geographic_msgs::RouteNetworkConstPtr &msg) {
    ...
    for (const auto &point : msg->points) {                             // rangefor combined with indirection -- `msg -> point` captured by new extractor
        const auto iter = std::find_if(point.props.begin(),             // using stl functions -- `point -> iter` is captured by new extractor
                                       point.props.end(),
                                       [](const geographic_msgs::KeyValue &p) {
                                           return p.key == "anm_waypoint";
                                       });
        }
        ...
    }
    ...
}
```

```
void WaypointsCollectionNodelet::saveLastState() {
    ...
    point.course = atan2(diff_y, diff_x) * 180 / M_PI; // correctly-guessed return value of library function:
                                                       // diff_y -> point, diff_x -> point captured by new dataflow extractor
    ...
}
```

