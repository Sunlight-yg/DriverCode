<?php session_start() ?>
<!DOCTYPE html>
<html>
<head>
    <meta http-equiv="context-type" context="text/html" charset="utf-8" />
    <title>阳光</title>
    <link rel="shortcut icon" href="img/sunlight.png" />
    <link rel="stylesheet" type="text/css" href="css/userText.css" />
    <link rel="stylesheet" type="text/css" href="css/index.css" />
    <style type="text/css">
    .top .nav ul .about {
  		background: rgba(247,87,51,1);
	}
    </style>
    <?php if(!is_null($_SESSION['username'])) echo "<link rel='stylesheet' type='text/css' href='css/index.icon.css' />" ?>
</head>
<body>
    <div class="top">
        <div class="outNick">
            <div class="nick">
                关于
            </div>
        </div>
        <div class="outNav">
            <div class="nav">
                <ul>
                    <li>
                        <a href="index.php">首页</a>
                    </li>
                    <li>
                        <a class="photos" href="photos.php">相册</a>
                    </li>
                    <li class="about">
                        <a href="about.php">关于</a>
                    </li>
                    <li class="icon">
                        <a <?php if(!is_null($_SESSION['username'])){ echo "href='userText.php'"; }else{ echo "href='login.html'";  } echo '>'; if(!is_null($_SESSION['username'])){ echo "<img src='img/icon.png' height='35' />"; }else{ echo "登陆"; } ?>
                </a>
                    </li><?php if(!is_null($_SESSION['username'])) echo "<li class='exit'><a href='php/exit.php'>退出</a></li>" ?>
                </ul>
                </div>
            </div>
        </div>
        <div class="content">
            <h2 class="h">开发者声明</h2>
            <p>Always believe that something wonderful is about to happen。</p>
            <p>永远相信美好的事情即将发生。</p>
        </div>
</body>
</html>