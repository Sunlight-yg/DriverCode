<?php 
include_once "php/include.php";
if(is_null($_SESSION['username']))
	skip('请登录!', '/login.html');
?>
<!DOCTYPE html>
<html>
<head>
    <meta http-equiv="context-type" context="text/html" charset="utf-8" />
    <meta HTTP-EQUIV="pragma" CONTENT="no-cache"> 
	<meta HTTP-EQUIV="Cache-Control" CONTENT="no-cache, must-revalidate"> 
	<meta HTTP-EQUIV="expires" CONTENT="0">
    <title>阳光</title>
    <link rel="shortcut icon" href="img/sunlight.png" />
    <link rel="stylesheet" type="text/css" href="css/index.css" />
    <link rel="stylesheet" type="text/css" href="css/photos.css" />
    <script type="text/javascript" src="https://code.jquery.com/jquery-3.3.1.min.js"></script>
    <?php if(!is_null($_SESSION['username'])) echo "<link rel='stylesheet' type='text/css' href='css/index.icon.css' />" ?>
</head>
<body>
    <div class="top">
        <div class="outNick">
            <div class="nick">
               	相册
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
        <div class="content">
            <h2 class="h">图片上传</h2>
            <form action="php/upPhotos.php" method="post" enctype="multipart/form-data">
    			<label for="file">文件名：</label>
    			<input class="sele" type="file" id="file" name="file[]"  multiple>
   				<input class="sub" type="submit" name="submit" value="提交">
			</form>
			<div class="imgs">
				<div class="div">
					<?php 
						$dir = "upload/";
						$arr = [];
						$username = md5($_SESSION['username']);
						read_all($dir.$username,$arr);
						foreach($arr as $name)
						{
							$filePath = $dir. $username ."/".$name;
							echo "
							<div class='imgDiv'>
								<a href=$filePath target='_blank'><img class='img' src=$filePath /></a>
								<div class='func'>
									<form action='php/chageImg.php' method='post' style='display: inline-block' enctype='multipart/form-data'>
    									<input class='file' type='file' name='file' style='display: none;' />
										<button class='choose' type='button' >修改</button>
   										<input type='submit' name='localImgName' style='display: none;' value=$name>
									</form>
									<a href=$filePath download='img'><button class='blue''>下载</button></a>
									<form action='php/delImg.php' method='post' style='display: inline-block'>
										<input type='hidden' name='delete' value=$name />
										<button class='red' type='submit'>删除</button>
									</form>
								</div>
							</div>";
						}
					?>
				</div>
			</div>
		</div>
    </div>
    <script type="application/javascript">
    	$(document).ready(init);
        function init() {
            $(".imgDiv").mouseenter(function () {
                $(this).find(".func").show();
 
            });
 
            $(".imgDiv").mouseleave(function () {
                $(this).find(".func").hide();
            });
        }
       	
       	var choose = document.getElementsByClassName('choose');
       	var fileArr = document.getElementsByClassName('file');
       	for(var i = 0; i != choose.length; i++)
       	{
       		choose[i].addEventListener('click', function(){
       			this.parentNode.firstElementChild.click();
       		});
       		fileArr[i].addEventListener('change', function(){
       			this.nextElementSibling.nextElementSibling.click();
       		});
       	}
    </script>
</body>
</html>

