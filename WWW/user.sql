CREATE TABLE `user` (
  
`id` int(11) NOT NULL AUTO_INCREMENT,
  
`username` varchar(30) NOT NULL,
  
`passwd` varchar(255) NOT NULL,
  
`nickname` varchar(50) DEFAULT '��',
  
`email` varchar(50) DEFAULT '��',
  
`signature` varchar(255) DEFAULT '��',  
`imgindex` int not null default 0,
  
PRIMARY KEY (`id`)
) ENGINE=MyISAM AUTO_INCREMENT=17 DEFAULT CHARSET=utf8