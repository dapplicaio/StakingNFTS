#include <game.hpp>

void game::receive_asset_transfer
(
  const name& from,
  const name& to,
  std::vector <uint64_t>& asset_ids,
  const std::string& memo
)
{
  if(to != get_self())
    return;

  if(memo == "stake farming item")
  {
    check(asset_ids.size() == 1, "You must transfer only one farming item to stake");
    stake_farmingitem(from, asset_ids[0]);
  }
  else if(memo.find("stake items:") != std::string::npos)
  {
    const uint64_t farmingitem_id = std::stoll(memo.substr(12));
    stake_items(from, farmingitem_id, asset_ids);
  }
  else
    check(0, "Invalid memo");
  
}


void game::stake_farmingitem(const name& owner, const uint64_t& asset_id)
{
    auto assets       = atomicassets::get_assets(get_self());
    auto asset_itr    = assets.find(asset_id);

    auto farmingitem_mdata = get_mdata(asset_itr);
    if(farmingitem_mdata.find("slots") == std::end(farmingitem_mdata))
    {
        auto farmingitem_template_idata = get_template_idata(asset_itr->template_id, asset_itr->collection_name);
        check(farmingitem_template_idata.find("maxSlots") != std::end(farmingitem_template_idata),
            "Farming item slots was not initialized. Contact ot dev team");
        check(farmingitem_template_idata.find("stakeableResources") != std::end(farmingitem_template_idata),
            "stakeableResources items at current farming item was not initialized. Contact to dev team");

        farmingitem_mdata["slots"] = (uint8_t)1;
        farmingitem_mdata["level"] = (uint8_t)1;

        update_mdata(asset_itr, farmingitem_mdata, get_self());
    }
    
    staked_t staked_table(get_self(), owner.value);
    staked_table.emplace(get_self(), [&](auto &new_row)
    {
        new_row.asset_id = asset_id;
    });
}


void game::stake_items(const name& owner, const uint64_t& farmingitem, const std::vector<uint64_t>& items_to_stake)
{
    auto assets       = atomicassets::get_assets(get_self());

    staked_t staked_table(get_self(), owner.value);
    auto staked_table_itr = staked_table.require_find(farmingitem, "Could not find farming staked item");
    auto asset_itr = assets.find(farmingitem);

    auto farmingitem_mdata          = get_mdata(asset_itr);
    auto farmingitem_template_idata = get_template_idata(asset_itr->template_id, asset_itr->collection_name); 

    check(std::get<uint8_t>(farmingitem_mdata["slots"]) >= staked_table_itr->staked_items.size() + items_to_stake.size(),
     "You don't have empty slots on current farming item to stake this amount of items");

    atomicdata::string_VEC stakeableResources = std::get<atomicdata::string_VEC>(farmingitem_template_idata["stakeableResources"]);
    for(const uint64_t& item_to_stake : items_to_stake)
    {
        asset_itr = assets.find(item_to_stake);
        auto item_mdata = get_mdata(asset_itr);

        item_mdata["lastClaim"] = current_time_point().sec_since_epoch();
        auto template_idata = get_template_idata(asset_itr->template_id, asset_itr->collection_name);
        if(item_mdata.find("level") == std::end(item_mdata))
        {
            check(template_idata.find("farmResource") != std::end(template_idata),
                "farmResource at item[" + std::to_string(item_to_stake) + "] was not initialized. Contact to dev team");
            check(template_idata.find("miningRate") != std::end(template_idata),
                "miningRate at item[" + std::to_string(item_to_stake) + "] was not initialized. Contact to dev team");
            check(template_idata.find("maxLevel") != std::end(template_idata),
                "maxLevel at item[" + std::to_string(item_to_stake) + "] was not initialized. Contact to dev team");
            
            item_mdata["level"] = (uint8_t)1;
        }

        check(std::find(std::begin(stakeableResources), std::end(stakeableResources), std::get<std::string>(template_idata["farmResource"])) != std::end(stakeableResources),
            "Item [" + std::to_string(item_to_stake) + "] can not be staked at current farming item");
        update_mdata(asset_itr, item_mdata, get_self());
    }

    staked_table.modify(staked_table_itr, get_self(), [&](auto &new_row)
    {
        new_row.staked_items.insert(std::end(new_row.staked_items), std::begin(items_to_stake), std::end(items_to_stake));
    });
}


atomicassets::ATTRIBUTE_MAP game::get_mdata(atomicassets::assets_t::const_iterator& assets_itr)
{
  auto schemas = atomicassets::get_schemas(assets_itr->collection_name);
  auto schema_itr = schemas.find(assets_itr->schema_name.value);

  atomicassets::ATTRIBUTE_MAP deserialized_mdata = atomicdata::deserialize             
  (
    assets_itr->mutable_serialized_data,
    schema_itr->format
  );

  return deserialized_mdata;
}

atomicassets::ATTRIBUTE_MAP game::get_template_idata(const int32_t& template_id, const name& collection_name)
{
  auto templates = atomicassets::get_templates(collection_name);
  auto template_itr = templates.find(template_id);

  auto schemas = atomicassets::get_schemas(collection_name);
  auto schema_itr = schemas.find(template_itr->schema_name.value);

  return atomicdata::deserialize             
  (
    template_itr->immutable_serialized_data,
    schema_itr->format
  );
}

void game::update_mdata(atomicassets::assets_t::const_iterator& assets_itr, const atomicassets::ATTRIBUTE_MAP& new_mdata, const name& owner)
{
  action
  (
    permission_level{get_self(),"active"_n},
    atomicassets::ATOMICASSETS_ACCOUNT,
    "setassetdata"_n,
    std::make_tuple
    (
      get_self(),
      owner,
      assets_itr->asset_id,
      new_mdata
    )
  ).send();
}





