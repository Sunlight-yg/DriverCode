<?php
include_once "include.php";
$name = $_POST['delete'];
$username = md5($_SESSION['username']);
$dir = $_SESSION['root'] . "upload\\" . $username . "\\" . $name;
if(unlink($dir))
	header('Location: /photos.php');
else
	skip('删除失败!', '/photos.php');
?>