myid = 9999;
moveNum = 999;
dir = 999;
targetID = 9999;

function set_uid(x)
	myid = x;
	moveNum = 0;
	moving = 0;
	return myid;
end

function player_move_notify(pid,direction)
	player_x = API_get_x(pid);
	player_y = API_get_y(pid);
	my_x = API_get_x(myid);
	my_y = API_get_y(myid);
	
	if (player_x == my_x) then
		if (player_y == my_y) then
			targetID = pid;
			API_SendMessage_Hello(myid, pid, "HELLO");	
			return 1;
		end
	end
end

function NPC_move()
	moveNum = moveNum + 1;
	API_Move_NPC(myid,dir);

	if(moveNum>=3) then
		API_SendMessage_Hello(myid,targetID,"Bye");
		moveNum = 0;
		moving = 0;
		return 1;
	end
end
	