<?php
include_once "include.php";
$username = md5($_SESSION['username']);
$dir = $_SESSION['root'] . "upload\\";
if(!is_dir($dir))
    mkdir($dir,true);
$dir .= $username . "\\";
$format = array("gif", "jpeg", "jpg", "png");
$res = true;
$imgindex = $link->getRow($table,$_SESSION['username'])['imgindex'];
if(!is_dir($dir))
    mkdir($dir,true);
//检测文件格式

for($i = 0, $j = count($_FILES["file"]["name"]); $i < $j; $i++){
    $temp = explode(".", $_FILES["file"]["name"][$i]);
	$extension = end($temp);
	if (!in_array($extension, $format))
	{
		skip("非法的文件格式", '/photos.php');
		return;		
	}
	if ($_FILES["file"]["error"][$i])
	{
		skip("第".($i + 1)."个文件发生错误:".$_FILES["file"]["error"][$i], '/photos.php');
		return;
	}	
}
//上传
for($i = 0, $j = count($_FILES["file"]["name"]); $i < $j; $i++){
	$szType = explode('/', $_FILES["file"]["type"][$i]);
	$szType = end($szType);
 	if(!move_uploaded_file($_FILES["file"]["tmp_name"][$i], $dir . "\\" . $imgindex . "." . $szType))
 		$res = false;
 	else {
 		$imgindex++;
 		$arr = compact('imgindex');
		$username=$_SESSION['username'];
 		$link->update($table, $arr, $username);
 	}
}
if($res)
	skip('上传成功。', '/photos.php');
else
	skip('有文件上传失败!', '/photos.php');
