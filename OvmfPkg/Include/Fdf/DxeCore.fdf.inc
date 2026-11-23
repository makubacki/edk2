!if $(PATINA_ENABLED) == FALSE
  INF  MdeModulePkg/Core/Dxe/DxeMain.inf
  INF  UefiCpuPkg/CpuDxe/CpuDxe.inf
  INF  FILE_GUID = $(UP_CPU_DXE_GUID) UefiCpuPkg/CpuDxe/CpuDxe.inf
!else
  FILE DXE_CORE = 23C9322F-2AF2-476A-BC4C-26BC88266C71 {
    !if $(TARGET) == DEBUG
      SECTION PE32 = $(DXE_CORE_PATH)/debug/qemu_ovmf_dxe_core.efi
    !else
      SECTION PE32 = $(DXE_CORE_PATH)/release/qemu_ovmf_dxe_core.efi
    !endif
    SECTION UI = "DxeCore"
  }
!endif
