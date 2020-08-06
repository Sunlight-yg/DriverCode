<?php
include_once 'include.php';
$username=$_SESSION['username'];
$nickname=$_POST['nickname'];
$email=$_POST['email'];
$signature=$_POST['signature'];
$array=compact('nickname','email','signature');
$res=$link->update($table,$array,$username);
if($res){
    skip('修改成功。','/userText.php');
}else{
    skip('修改失败!','/userTextEdit.php');
}
