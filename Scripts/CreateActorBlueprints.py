import unreal

def create_blueprint_class(parent_class_name: str, destination_path: str, blueprint_name: str):
    """
    Creates a Blueprint asset inheriting from the specified native C++ parent class.
    """
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    
    # 1. Load Parent C++ Class
    # In UE, classes in the project module are located at /Script/<ModuleName>.<ClassName>
    class_path = f"/Script/scratchanimal.{parent_class_name}"
    parent_class = unreal.load_class(None, class_path)
    
    if not parent_class:
        unreal.log_error(f"[CreateBP] Failed to find class '{class_path}'. Ensure C++ code is compiled first!")
        return None

    # 2. Setup Blueprint Factory
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)

    # 3. Create the Blueprint Asset
    new_asset = asset_tools.create_asset(
        asset_name=blueprint_name,
        package_path=destination_path,
        asset_class=unreal.Blueprint,
        factory=factory
    )

    if new_asset:
        unreal.EditorAssetLibrary.save_loaded_asset(new_asset)
        unreal.log(f"[CreateBP] Successfully created and saved: {destination_path}/{blueprint_name}")
    else:
        unreal.log_warning(f"[CreateBP] Asset creation skipped or failed for: {destination_path}/{blueprint_name}")

    return new_asset


def main():
    dest_path = "/Game/Actor"
    
    # Ensure Content/Actor folder exists
    if not unreal.EditorAssetLibrary.does_directory_exist(dest_path):
        unreal.EditorAssetLibrary.make_directory(dest_path)

    # Create BP_SAPlayerPawn
    create_blueprint_class("SAPlayerPawn", dest_path, "BP_SAPlayerPawn")

    # Create BP_SAMonsterActor
    create_blueprint_class("SAMonsterActor", dest_path, "BP_SAMonsterActor")


if __name__ == "__main__":
    main()
