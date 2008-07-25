drop table if exists media_items;
drop table if exists media_list_types;
drop table if exists properties;
drop table if exists resource_properties;
drop table if exists library_metadata;
drop table if exists simple_media_lists;
drop table if exists library_media_item;
drop table if exists resource_properties_fts;
drop table if exists resource_properties_fts_all;

create table library_metadata (
  name text primary key,
  value text
);

create table media_items (
  media_item_id integer primary key autoincrement,  /* implicit index creation */
  guid text unique not null, /* implicit index creation */
  created integer not null,
  updated integer not null,
  content_url text not null,
  content_mime_type text,
  content_length integer,
  content_hash text,
  hidden integer not null check(hidden in (0, 1)),
  media_list_type_id integer
);
create index idx_media_items_hidden on media_items (hidden);
create index idx_media_items_created on media_items (created);
create index idx_media_items_content_url on media_items (content_url);
create index idx_media_items_content_hash on media_items (content_hash);
create index idx_media_items_content_url_content_hash on media_items (content_url, content_hash);
create index idx_media_items_media_list_type_id on media_items (media_list_type_id);
create index idx_media_items_hidden_media_list_type_id on media_items (hidden, media_list_type_id);

create table library_media_item (
  guid text unique not null, /* implicit index creation */
  created integer not null,
  updated integer not null,
  content_url text not null,
  content_mime_type text,
  content_length integer,
  content_hash text,
  hidden integer not null check(hidden in (0, 1)),
  media_list_type_id integer
);

create table media_list_types (
  media_list_type_id integer primary key autoincrement, /* implicit index creation */
  type text unique not null, /* implicit index creation */
  factory_contractid text not null
);

create table properties (
  property_id integer primary key autoincrement, /* implicit index creation */
  property_name text not null unique /* implicit index creation */
);

create table resource_properties (
  media_item_id integer not null,
  property_id integer not null,
  obj text not null,
  obj_sortable text,
  primary key (media_item_id, property_id)
);
create index idx_resource_properties_property_id_obj on resource_properties (property_id, obj);
create index idx_resource_properties_obj_sortable on resource_properties (obj_sortable);
create index idx_resource_properties_media_item_id_property_id_obj_sortable on resource_properties (media_item_id, property_id, obj_sortable);
create index idx_resource_properties_property_id_obj_sortable_media_item_id on resource_properties (property_id, obj_sortable, media_item_id);

create table simple_media_lists (
  media_item_id integer not null,
  member_media_item_id integer not null,
  ordinal text not null collate tree
);
create index idx_simple_media_lists_media_item_id_member_media_item_id on simple_media_lists (media_item_id, member_media_item_id, ordinal);
create unique index idx_simple_media_lists_media_item_id_ordinal on simple_media_lists (media_item_id, ordinal);
create index idx_simple_media_lists_member_media_item_id on simple_media_lists (member_media_item_id);

/* resource_properties_fts is disabled. See bug 9488 and bug 9617. */
/* create virtual table resource_properties_fts using FTS3 (propertyid, obj); */

create virtual table resource_properties_fts_all using FTS3 (
  alldata
);

/* note the empty comment blocks at the end of the lines in the body of the */
/* trigger need to be there to prevent the parser from splitting on the */
/* line ending semicolon */
   
/* We can reinsert this when we start using the FTS3 resource_properties_fts table again. */
/* delete from resource_properties_fts where rowid in (select rowid from resource_properties where media_item_id = OLD.media_item_id) */
create trigger tgr_media_items_simple_media_lists_delete before delete on media_items
begin
  delete from resource_properties_fts_all where rowid = OLD.media_item_id; /**/
  delete from simple_media_lists where member_media_item_id = OLD.media_item_id or media_item_id = OLD.media_item_id; /**/
  delete from resource_properties where media_item_id = OLD.media_item_id; /**/
end;

/* static data */

insert into properties (property_name) values ('http://songbirdnest.com/data/1.0#trackName');
insert into properties (property_name) values ('http://songbirdnest.com/data/1.0#albumName');
insert into properties (property_name) values ('http://songbirdnest.com/data/1.0#artistName');
insert into properties (property_name) values ('http://songbirdnest.com/data/1.0#duration');
insert into properties (property_name) values ('http://songbirdnest.com/data/1.0#genre');
insert into properties (property_name) values ('http://songbirdnest.com/data/1.0#trackNumber');
insert into properties (property_name) values ('http://songbirdnest.com/data/1.0#year');
insert into properties (property_name) values ('http://songbirdnest.com/data/1.0#discNumber');
insert into properties (property_name) values ('http://songbirdnest.com/data/1.0#totalDiscs');
insert into properties (property_name) values ('http://songbirdnest.com/data/1.0#totalTracks');
insert into properties (property_name) values ('http://songbirdnest.com/data/1.0#lastPlayTime');
insert into properties (property_name) values ('http://songbirdnest.com/data/1.0#playCount');
insert into properties (property_name) values ('http://songbirdnest.com/data/1.0#customType');
insert into properties (property_name) values ('http://songbirdnest.com/data/1.0#isSortable');

insert into media_list_types (type, factory_contractid) values ('simple', '@songbirdnest.com/Songbird/Library/LocalDatabase/SimpleMediaListFactory;1');

/* XXXAus: !!!WARNING!!! When changing this value, you _MUST_ update sbLocalDatabaseMigration._latestSchemaVersion to match this value */
insert into library_metadata (name, value) values ('version', '5');
