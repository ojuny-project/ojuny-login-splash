self:
{ config, lib, pkgs, ... }:

let
  cfg = config.services.ojuny-login-splash;
  arguments = lib.concatStringsSep " " (
    lib.optional (cfg.seconds != null) "--seconds ${toString cfg.seconds}"
    ++ lib.optional (cfg.fade != null) "--fade ${toString cfg.fade}"
  );
in
{
  options.services.ojuny-login-splash = {
    enable = lib.mkEnableOption "the Ojuny login splash";

    package = lib.mkOption {
      type = lib.types.package;
      default = self.packages.${pkgs.stdenv.hostPlatform.system}.default;
      description = "Package providing ojuny-login-splash.";
    };

    seconds = lib.mkOption {
      type = lib.types.nullOr lib.types.float;
      default = 10.0;
      description = "Exit after this many seconds. Null keeps the splash until ESC or --quit.";
    };

    showOnRebuild = lib.mkOption {
      type = lib.types.bool;
      default = false;
      description = "Start the splash on every service start, including rebuilds. When false, it starts once per boot or login.";
    };

    fade = lib.mkOption {
      type = lib.types.nullOr lib.types.float;
      default = null;
      description = "Fade and grow over this many seconds instead of rotating.";
    };

    sessionTarget = lib.mkOption {
      type = lib.types.str;
      default = "mango-session.target";
      description = "Systemd user target that starts the splash.";
    };
  };

  config = lib.mkIf cfg.enable {
    systemd.user.services.ojuny-login-splash = {
      Unit = {
        Description = "Ojuny login splash";
        PartOf = [ cfg.sessionTarget ];
        After = [ "graphical-session-pre.target" ];
        Before = [ "xdg-desktop-autostart.target" ];
        ConditionPathExists =
          lib.mkIf (!cfg.showOnRebuild) [ "!%t/ojuny-login-splash.started" ];
      };

      Service = {
        Type = "exec";
        ExecStartPre =
          lib.mkIf (!cfg.showOnRebuild)
            "${pkgs.coreutils}/bin/touch %t/ojuny-login-splash.started";
        ExecStart = "${cfg.package}/bin/ojuny-login-splash ${arguments}";
        ExecStop = "${cfg.package}/bin/ojuny-login-splash --quit";
        Restart = "no";
        TimeoutStopSec = 1;
      };

      Install.WantedBy = [ cfg.sessionTarget ];
    };
  };
}
