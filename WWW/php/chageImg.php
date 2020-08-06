<?php
header("Cache-Control: no-cache, must-revalidate");
include_once "include.php";
$localImgName = $_POST['localImgName'];
$username = md5($_SESSION['username']);
$dir = $_SESSION['root'] . "upload\\" . $username . "\\" . $localImgName;
$imgindex = $link->getRow($table,$_SESSION['username'])['imgindex'];
$format = array("gif", "jpeg", "jpg", "png");
$res = true;

//检测文件格式
$temp = explode(".", $_FILES["file"]["name"]);
$extension = end($temp);
if (!in_array($extension, $format))
{
	skip("非法的文件格式", '/photos.php');
	return;
}
if ($_FILES["file"]["error"])
{
	skip("文件发生错误:".$_FILES["file"]["error"], '/photos.php');
	return;
}

//删除文件
if(unlink($dir)) {
	//文件上传
	if(!move_uploaded_file($_FILES["file"]["tmp_name"], $dir))
		$res = false;
	else {
		$arr = compact('imgindex');
		$username=$_SESSION['username'];
 		$link->update($table, $arr, $username);
	}
}
if($res)
	header('Location: /photos.php');
else
	skip('修改失败!', '/photos.php');
?>