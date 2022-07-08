
/*!40101 SET NAMES utf8 */;

/*!40101 SET SQL_MODE=''*/;

/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

-- Alter gossip_texts to add WordId field
ALTER TABLE `gossip_texts` ADD `WordId` INT(11) DEFAULT 0  COMMENT 'Lookup into trans_words' AFTER `comment`;
ALTER TABLE `gossip_texts` ADD `comment_default` TEXT DEFAULT NULL  COMMENT 'temp storage of Content_default' AFTER `WordId`;

-- Populate comment_default with the original content_default for now
UPDATE IGNORE `gossip_texts` SET `comment_default`=`content_default`;

-- Update the WordId field
UPDATE `gossip_texts` set WordId=127678 where entry=-3469001;
UPDATE `gossip_texts` set WordId=127679 where entry=-3469000;
UPDATE `gossip_texts` set WordId=127681 where entry=-3409001;
UPDATE `gossip_texts` set WordId=127682 where entry=-3230002;
UPDATE `gossip_texts` set WordId=127683 where entry=-3409000;
UPDATE `gossip_texts` set WordId=127684 where entry=-3230001;
UPDATE `gossip_texts` set WordId=127685 where entry=-3230000;
UPDATE `gossip_texts` set WordId=127686 where entry=-3043000;
UPDATE `gossip_texts` set WordId=127687 where entry=-3090000;
UPDATE `gossip_texts` set WordId=127688 where entry=-3033000;
UPDATE `gossip_texts` set WordId=127690 where entry=-3000109;
UPDATE `gossip_texts` set WordId=127691 where entry=-3000108;
UPDATE `gossip_texts` set WordId=127692 where entry=-3000107;
UPDATE `gossip_texts` set WordId=127693 where entry=-3000106;
UPDATE `gossip_texts` set WordId=127694 where entry=-3000105;
UPDATE `gossip_texts` set WordId=127695 where entry=-3000000;
UPDATE `gossip_texts` set WordId=127696 where entry=-3469002;
UPDATE `gossip_texts` set WordId=127697 where entry=-3469003;
UPDATE `gossip_texts` set WordId=127698 where entry=-3469004;
UPDATE `gossip_texts` set WordId=127699 where entry=-3509000;
UPDATE `gossip_texts` set WordId=127700 where entry=-3509001;
UPDATE `gossip_texts` set WordId=127701 where entry=-3230003;
UPDATE `gossip_texts` set WordId=127702 where entry=-3000111;
UPDATE `gossip_texts` set WordId=129272 where entry=-3000110;
UPDATE `gossip_texts` set WordId=354350 where entry=-3509004;
UPDATE `gossip_texts` set WordId=354350 where entry=-3409002;
UPDATE `gossip_texts` set WordId=354351 where entry=-3509003;
UPDATE `gossip_texts` set WordId=354352 where entry=-3509002;

-- TODO: Remove content_ fields
ALTER TABLE `gossip_texts` DROP `Content_default`;
ALTER TABLE `gossip_texts` DROP `Content_loc1`;
ALTER TABLE `gossip_texts` DROP `Content_loc2`;
ALTER TABLE `gossip_texts` DROP `Content_loc3`;
ALTER TABLE `gossip_texts` DROP `Content_loc4`;
ALTER TABLE `gossip_texts` DROP `Content_loc5`;
ALTER TABLE `gossip_texts` DROP `Content_loc6`;
ALTER TABLE `gossip_texts` DROP `Content_loc7`;
ALTER TABLE `gossip_texts` DROP `Content_loc8`;


UPDATE `gossip_texts` SET `wordid`=283315 WHERE `entry`=-3409002;