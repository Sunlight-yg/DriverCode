<?php
include_once 'include.php';
$username=$_POST['username'];
$passwd=md5($_POST['passwd']);
$passwd2=md5($_POST['passwd2']);
$array=compact("username","passwd");
if($passwd==$passwd2){
    $row=$link->getRow($table,$username);
    if($row){
        skip('账号已存在!','/register.html');
    }else{
        $res=$link->insert($table,$array);
        if($res){
            skip('注册成功!','/login.html');
        }else{
            skip('注册失败!','/register.html');
        }
    }
}else
    skip('两次密码不一致!','/register.html');

