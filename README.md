# Balloon Fight

# Game Overview

# Game Controls

# Tech Setup

# Bugs
There is some weird bug in level one. Once you kill all the enemies, and there are balloons left over, there is one spot on the map where you cannot collect the balloon. On the far right side, right by the wall, if you try to collect a balloon after all enemies are gone, you will just push the balloon up. It's only in that spot, and its only after all enemies are dead. I think it has something to do with past logic, where I had ladders. In the collison map, if you collided with blue, it meant you were on a ladder and that had some logic to it. Maybe some of that lingered. It was palette index 4, (0,0,31).