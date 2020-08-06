<!DOCTYPE html>
<html>
<head>
    <meta http-equiv="context-type" context="text/html" charset="utf-8" />
    <title>阳光</title>
    <link rel="shortcut icon" href="img/sunlight.png" />
    <link rel="stylesheet" type="text/css" href="css/index.css" />
    <link rel='stylesheet' type='text/css' href='css/index.icon.css' />
    <link rel="stylesheet" type="text/css" href="css/userText.css" />
    <link rel="stylesheet" type="text/css" href="css/userTextEdit.css" />
</head>
<?php
include_once 'php/include.php';
$table='user';
$arr=$link->getRow($table,$_SESSION['username']);
?>
<body>
    <div class="top">
        <div class="outNick">
            <div class="nick">
                <?php echo "{$arr['nickname']}的个人空间" ?>
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
                        <a href="userText.php">
                            <img src="img/icon.png" height="35" />
                        </a>
                    </li>
                    <li class="exit">
                        <a href="php/exit.php">退出</a>
                    </li>
                </ul>
            </div>
        </div>
    </div>
    <div class="content">
        <h2 class="h">个人资料</h2>
        <div class="data">
            <form action="php/edit.php" method="post">
                <p>
                    账号：<?php echo $arr['username'] ?>
                </p>
                <p>
                    昵称：
                    <input class='in' type='text' name='nickname' value='<?php echo $arr['nickname'] ?>' />
                </p>
                <p>
                    邮箱：
                    <input class='in' type='email' name='email' value='<?php echo $arr['email'] ?>' />
                </p>
                个性签名：
                <p class="signature">
                    <textarea class="signatureIn" name="signature"><?php echo $arr['signature'] ?></textarea>
                </p>
                <input class="mit" type="submit" value="保存" />
                <input id="cancel" class="mit" type="button" onclick="window.location.href='userText.php'" value="取消" />
            </form>
        </div>
    </div>
</body>
</html>
