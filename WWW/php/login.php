<?php
include_once 'include.php';
$username=$_POST['username'];
$passwd=md5($_POST['passwd']);
$res=$link->select($table,$username,$passwd);
if($res){
    $_SESSION['username']=$username;
    header("location: /index.php");
}else{
    skip('账号或密码错误！','/login.html');
}